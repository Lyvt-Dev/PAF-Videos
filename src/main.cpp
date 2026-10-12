#include "platform.hpp"
#include "media.hpp"
#include "updater.hpp"
#include "version.h"
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <fstream>
#include <sstream>
#include <memory>
#include <thread>
#include <random>
#include <stdexcept>
#include <chrono>
#include <cwctype>

using namespace Gdiplus;
constexpr UINT WM_UPDATE=WM_APP+1,WM_LIBRARY=WM_APP+2;
enum Page {Home,Music,Video,Queue,Playlists,Settings,Playback};
enum Command {NavToggle=100,OpenFiles,OpenFolder,PlayPause,Previous,Next,Shuffle,Repeat,Mute,FullScreen,Subtitle,AudioTrack,Speed,ClearQueue,SavePlaylist,DeletePlaylist,AddMusic,AddVideo,ManageMusic,ManageVideo,Refresh,Theme,Accent,Remember,ClearRecent,Automatic,CheckUpdate,InstallUpdate,Releases,ShowHome=200,ShowMusic,ShowVideo,ShowQueue,ShowPlaylists,ShowSettings,ShowPlayback};
struct Button {RECT rect{};int command=0;std::wstring label;bool selected=false,accent=false;};
struct LibraryResult {bool audio;std::vector<fs::path> paths;};
struct App {
    HWND window=nullptr,surface=nullptr,list=nullptr,search=nullptr,playlistCombo=nullptr,seek=nullptr,volume=nullptr;
    fs::path directory,data,ini;MediaPlayer player;UpdateService updater;std::unique_ptr<Bitmap> logo;
    std::vector<Button> buttons;std::vector<fs::path> queue,recent,musicFolders,videoFolders,musicLibrary,videoLibrary,listPaths,playlistPaths;
    std::vector<std::thread> workers;std::vector<int> displayed;std::vector<fs::path> playlistFiles;
    Page page=Home;int index=-1,volumeValue=80,accent=0,theme=0,settingsScroll=0,lastState=0,speedIndex=2;
    bool expanded=false,shuffle=false,repeat=false,muted=false,remember=true,automatic=true,fullscreen=false,dragSeek=false,test=false,normalProbe=false,closing=false;
    bool scanningMusic=false,scanningVideo=false;RECT restoreRect{};DWORD restoreStyle=0;
    float scale=1;std::wstring filter,updateText=L"Noch nicht geprüft";fs::path report,fixture;int selfStage=0;
    ULONGLONG started=0,nextUpdate=0;std::mt19937 random{std::random_device{}()};HFONT controlFont=nullptr;
    COLORREF bg()const{return light()?RGB(247,247,247):RGB(37,39,42);}COLORREF card()const{return light()?RGB(233,233,233):RGB(48,50,53);}
    COLORREF fg()const{return light()?RGB(28,28,28):RGB(246,246,246);}COLORREF secondary()const{return light()?RGB(90,90,90):RGB(174,174,174);}
    COLORREF orange()const{const COLORREF values[]={RGB(255,130,43),RGB(167,108,245),RGB(59,179,242),RGB(65,197,144)};return values[accent%4];}
    bool light()const{
        if(theme!=2)return theme==1;DWORD value=0,size=sizeof(value);RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&size);return value!=0;
    }
    int d(int value)const{return int(value*scale+.5f);}int setting(const wchar_t* key,int fallback){return int(GetPrivateProfileIntW(L"general",key,fallback,ini.c_str()));}
    void save(const wchar_t* key,int value){WritePrivateProfileStringW(L"general",key,std::to_wstring(value).c_str(),ini.c_str());}
    void initializeData(){
        directory=executableDirectory();PWSTR local=nullptr;
        if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))throw std::runtime_error("LocalAppData unavailable");
        data=fs::path(local)/L"PAF-Videos";CoTaskMemFree(local);
        if(test)data=report.parent_path()/L"paf-self-test-state";
        fs::create_directories(data/L"playlists");ini=data/L"settings.ini";
        volumeValue=std::clamp(setting(L"volume",80),0,100);accent=std::clamp(setting(L"accent",0),0,3);theme=std::clamp(setting(L"theme",0),0,2);
        remember=setting(L"remember",1)!=0;automatic=!test&&setting(L"automatic",1)!=0;expanded=setting(L"expanded",0)!=0;
        recent=readPaths(data/L"recent.m3u8");musicFolders=readPaths(data/L"music-folders.txt");videoFolders=readPaths(data/L"video-folders.txt");
    }
    static std::vector<fs::path> readPaths(const fs::path& file){
        std::vector<fs::path> values;std::ifstream in(file,std::ios::binary);std::string line;
        while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#')continue;auto path=wide(line);if(!path.empty())values.emplace_back(path);}
        return values;
    }
    static void writePaths(const fs::path& file,const std::vector<fs::path>& values){
        auto temporary=file;temporary+=L".tmp";std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("Cannot save playlist or library settings");
        for(const auto& value:values)out<<utf8(value.wstring())<<'\n';out.close();if(!out)throw std::runtime_error("Failed to save settings");
        if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace settings file");
    }
    void loadLogo(){
        HRSRC resource=FindResourceW(nullptr,MAKEINTRESOURCEW(2),RT_RCDATA);if(!resource)throw std::runtime_error("Logo resource missing");
        auto loaded=LoadResource(nullptr,resource);auto bytes=SizeofResource(nullptr,resource);auto block=GlobalAlloc(GMEM_MOVEABLE,bytes);
        if(!block)throw std::bad_alloc();auto ptr=GlobalLock(block);CopyMemory(ptr,LockResource(loaded),bytes);GlobalUnlock(block);
        IStream* stream=nullptr;if(FAILED(CreateStreamOnHGlobal(block,TRUE,&stream))){GlobalFree(block);throw std::runtime_error("Cannot read logo resource");}
        auto source=Bitmap::FromStream(stream);if(source&&source->GetLastStatus()==Ok)logo.reset(source->Clone(0,0,source->GetWidth(),source->GetHeight(),PixelFormat32bppARGB));delete source;stream->Release();
        if(!logo)throw std::runtime_error("Cannot decode logo resource");
    }
    RECT client()const{RECT r{};GetClientRect(window,&r);return r;}
    int navWidth()const{return d(expanded?300:64);}int controlsTop()const{return client().bottom-d(144);}int contentLeft()const{return navWidth()+d(64);}
    void create(HWND owner){
        window=owner;scale=GetDpiForWindow(window)/96.f;loadLogo();
        controlFont=CreateFontW(-d(17),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        surface=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|SS_BLACKRECT,0,0,10,10,window,nullptr,nullptr,nullptr);
        list=CreateWindowExW(0,L"LISTBOX",L"",WS_CHILD|WS_VSCROLL|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT,0,0,10,10,window,(HMENU)300,nullptr,nullptr);
        search=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|ES_AUTOHSCROLL,0,0,10,10,window,(HMENU)301,nullptr,nullptr);
        SendMessageW(search,EM_SETCUEBANNER,0,(LPARAM)L"Suchen");
        playlistCombo=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,10,10,window,(HMENU)302,nullptr,nullptr);
        seek=CreateWindowExW(0,TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,0,0,10,10,window,(HMENU)303,nullptr,nullptr);
        volume=CreateWindowExW(0,TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,0,0,10,10,window,(HMENU)304,nullptr,nullptr);
        for(auto control:{list,search,playlistCombo})SendMessageW(control,WM_SETFONT,(WPARAM)controlFont,TRUE);
        SendMessageW(list,LB_SETITEMHEIGHT,0,d(52));SendMessageW(seek,TBM_SETRANGE,TRUE,MAKELPARAM(0,10000));SendMessageW(volume,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(volume,TBM_SETPOS,TRUE,volumeValue);
        player.initialize(surface,test);player.volume(volumeValue);DragAcceptFiles(window,TRUE);
        loadPlaylists();refreshList();layout();SetTimer(window,1,200,nullptr);started=GetTickCount64();nextUpdate=started+2000;
        if(!test){scanLibrary(true);scanLibrary(false);}
    }
    void layout(){
        auto r=client();int left=contentLeft(),right=r.right-d(64),top=d(165),bottom=controlsTop()-d(24);
        MoveWindow(search,d(22),d(75),navWidth()-d(44),d(40),TRUE);ShowWindow(search,expanded?SW_SHOW:SW_HIDE);
        int listTop=top;
        if(page==Home)listTop=std::min(bottom-d(150),d(590));
        else if(page==Music||page==Video||page==Queue||page==Playlists)listTop+=d(60);
        MoveWindow(list,left,listTop,std::max(d(100),right-left),std::max(d(40),bottom-listTop),TRUE);
        bool hasList=(page==Home&&!recent.empty())||page==Music||page==Video||page==Queue||page==Playlists;
        ShowWindow(list,hasList?SW_SHOW:SW_HIDE);
        MoveWindow(playlistCombo,left,top,std::max(d(120),right-left-d(220)),d(260),TRUE);ShowWindow(playlistCombo,page==Playlists?SW_SHOW:SW_HIDE);
        MoveWindow(surface,navWidth(),d(105),r.right-navWidth(),std::max(d(40),controlsTop()-d(105)),TRUE);ShowWindow(surface,page==Playback?SW_SHOW:SW_HIDE);
        MoveWindow(seek,d(94),controlsTop()+d(12),std::max(d(100),int(r.right)-d(210)),d(28),TRUE);
        MoveWindow(volume,r.right-d(150),controlsTop()+d(77),d(100),d(28),TRUE);
        BOOL dark=!light();DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
        InvalidateRect(window,nullptr,FALSE);
    }
    void text(HDC dc,std::wstring value,RECT rect,int size=17,bool bold=false,COLORREF color=CLR_INVALID,const wchar_t* family=L"Segoe UI",UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS){
        auto font=CreateFontW(-d(size),0,0,0,bold?FW_SEMIBOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,family);
        auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color==CLR_INVALID?fg():color);DrawTextW(dc,value.c_str(),int(value.size()),&rect,flags);SelectObject(dc,old);DeleteObject(font);
    }
    void fill(HDC dc,RECT r,COLORREF color){auto brush=CreateSolidBrush(color);FillRect(dc,&r,brush);DeleteObject(brush);}
    RECT rect(int x,int y,int w,int h)const{return {x,y,x+w,y+h};}
    void button(HDC dc,RECT r,int cmd,const std::wstring& label,bool selected=false,bool highlight=false){
        buttons.push_back({r,cmd,label,selected,highlight});
        if(highlight||selected){fill(dc,r,highlight?orange():card());if(selected&&!highlight)fill(dc,rect(r.left,r.top+d(12),d(4),r.bottom-r.top-d(24)),orange());}
        RECT labelRect=r;labelRect.left+=d(14);text(dc,label,labelRect,17,false,highlight?RGB(16,16,16):fg());
    }
    void paint(HDC target){
        auto r=client();auto buffer=CreateCompatibleDC(target);auto bitmap=CreateCompatibleBitmap(target,std::max(1L,r.right),std::max(1L,r.bottom));auto previous=SelectObject(buffer,bitmap);HDC dc=buffer;
        fill(dc,r,bg());buttons.clear();int nav=navWidth(),bottom=controlsTop();fill(dc,rect(0,0,nav,bottom),light()?RGB(237,237,237):RGB(31,32,34));
        button(dc,rect(d(7),d(14),nav-d(14),d(44)),NavToggle,expanded?L"☰   Navigation":L"☰");
        const wchar_t* names[]={L"Home",L"Musikbibliothek",L"Videobibliothek",L"Wiedergabewarteschlange",L"Wiedergabelisten"};
        const wchar_t* symbols[]={L"⌂",L"♫",L"▣",L"☷",L"▤"};
        int y=d(expanded?137:100);
        for(int i=0;i<5;++i){button(dc,rect(d(7),y+i*d(52),nav-d(14),d(46)),ShowHome+i,expanded?std::wstring(symbols[i])+L"   "+names[i]:symbols[i],page==i);}
        button(dc,rect(d(7),bottom-d(58),nav-d(14),d(46)),ShowSettings,expanded?L"⚙   Einstellungen":L"⚙",page==Settings);
        Graphics graphics(dc);graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);graphics.DrawImage(logo.get(),nav+d(16),d(14),d(26),d(26));
        text(dc,L"PAF-Videos",rect(nav+d(52),d(12),d(240),d(32)),15);
        std::wstring heading=page<5?names[page]:page==Settings?L"Einstellungen":(index>=0?queue[size_t(index)].filename().wstring():L"Wiedergabe");
        text(dc,heading,rect(contentLeft(),d(53),r.right-contentLeft()-d(44),d(82)),page==Playback?24:48,true);
        if(page==Home){
            int hero=std::min(d(320),std::max(d(190),(int(r.right)-nav-d(160))/3));int cy=std::max(d(200),(bottom-d(120)-hero)/2+d(90));int x=contentLeft()+d(20);
            graphics.DrawImage(logo.get(),x,cy,hero,hero);int tx=x+hero+d(40),tw=r.right-tx-d(55);
            text(dc,L"Dein neuer PAF-Videos Player",rect(tx,cy+d(55),tw,d(85)),30,true,CLR_INVALID,L"Segoe UI",DT_LEFT|DT_WORDBREAK);
            text(dc,L"Spiele deine Videos und Musik ab und entdecke deine persönlichen Bibliotheken.",rect(tx,cy+d(150),tw,d(65)),17,false,secondary(),L"Segoe UI",DT_LEFT|DT_WORDBREAK);
            button(dc,rect(tx,cy+d(225),d(204),d(44)),OpenFiles,L"▱  Datei(en) öffnen",false,true);
            button(dc,rect(tx+d(218),cy+d(225),d(165),d(44)),OpenFolder,L"Ordner öffnen");
        }else if(page==Music||page==Video){
            button(dc,rect(contentLeft(),d(162),d(190),d(42)),page==Music?AddMusic:AddVideo,L"Ordner hinzufügen",false,true);
            button(dc,rect(contentLeft()+d(200),d(162),d(180),d(42)),Refresh,L"Aktualisieren");
            if((page==Music?scanningMusic:scanningVideo))text(dc,L"Bibliothek wird durchsucht …",rect(contentLeft()+d(390),d(162),d(300),d(42)),15,false,secondary());
        }else if(page==Queue){
            button(dc,rect(contentLeft(),d(162),d(160),d(42)),ClearQueue,L"Leeren");button(dc,rect(contentLeft()+d(172),d(162),d(280),d(42)),SavePlaylist,L"Als Wiedergabeliste speichern",false,true);
        }else if(page==Playlists){button(dc,rect(r.right-d(254),d(162),d(190),d(42)),DeletePlaylist,L"Liste löschen");}
        else if(page==Settings)paintSettings(dc);
        fill(dc,rect(0,bottom,r.right,r.bottom-bottom),light()?RGB(232,232,232):RGB(40,39,38));
        text(dc,paf::clock(player.time()),rect(d(20),bottom+d(9),d(75),d(30)),14);
        text(dc,paf::clock(player.length()),rect(r.right-d(97),bottom+d(9),d(90),d(30)),14);
        if(index>=0)text(dc,queue[size_t(index)].filename().wstring(),rect(d(22),bottom+d(43),r.right-d(44),d(24)),14,false,secondary());
        int center=r.right/2;
        button(dc,rect(center-d(184),bottom+d(72),d(48),d(48)),Shuffle,L"⤨",shuffle);
        button(dc,rect(center-d(120),bottom+d(72),d(48),d(48)),Previous,L"⏮");
        button(dc,rect(center-d(40),bottom+d(62),d(80),d(62)),PlayPause,player.state()==3?L"Ⅱ":L"▶",false,true);
        button(dc,rect(center+d(70),bottom+d(72),d(48),d(48)),Next,L"⏭");
        button(dc,rect(center+d(134),bottom+d(72),d(48),d(48)),Repeat,L"↻",repeat);
        button(dc,rect(d(12),bottom+d(76),d(55),d(44)),Subtitle,L"CC");button(dc,rect(d(76),bottom+d(76),d(55),d(44)),AudioTrack,L"♫");
        const wchar_t* rates[]={L"0.5×",L"0.75×",L"1×",L"1.25×",L"1.5×",L"2×"};button(dc,rect(d(140),bottom+d(76),d(70),d(44)),Speed,rates[speedIndex]);
        button(dc,rect(r.right-d(218),bottom+d(72),d(55),d(48)),Mute,muted?L"🔇":L"🔊",muted);
        button(dc,rect(r.right-d(48),bottom+d(72),d(44),d(48)),FullScreen,L"⛶");
        BitBlt(target,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);SelectObject(buffer,previous);DeleteObject(bitmap);DeleteDC(buffer);
    }
    void paintSettings(HDC dc){
        int left=contentLeft(),width=client().right-left-d(64),y=d(162)-settingsScroll;int old=SaveDC(dc);IntersectClipRect(dc,left,d(145),left+width,controlsTop()-d(15));
        auto heading=[&](const std::wstring& title){text(dc,title,rect(left,y,width,d(35)),17,true);y+=d(40);};
        auto row=[&](const std::wstring& label,const std::wstring& value,int command){RECT box=rect(left,y,width,d(76));fill(dc,box,card());text(dc,label,rect(left+d(24),y+d(8),width-d(260),d(60)));button(dc,rect(left+width-d(230),y+d(15),d(215),d(46)),command,value);y+=d(82);};
        heading(L"Bibliotheken");row(L"Musikbibliothek – "+std::to_wstring(musicFolders.size())+L" Ordner",L"Ordner hinzufügen",AddMusic);row(L"Videobibliothek – "+std::to_wstring(videoFolders.size())+L" Ordner",L"Ordner hinzufügen",AddVideo);
        row(L"Bibliotheksordner verwalten",L"Ordner entfernen",ManageVideo);row(L"Bibliotheken aktualisieren",L"Aktualisieren",Refresh);y+=d(20);
        heading(L"Personalisierung");const wchar_t* themes[]={L"Dunkel",L"Hell",L"System"};const wchar_t* accents[]={L"Orange",L"Violett",L"Blau",L"Grün"};
        row(L"App-Design",themes[theme],Theme);row(L"Akzentfarbe",accents[accent],Accent);y+=d(20);
        heading(L"Datenschutz");row(L"Zuletzt geöffnete Medien merken",remember?L"Aktiviert":L"Deaktiviert",Remember);row(L"Medienverlauf löschen",L"Verlauf löschen",ClearRecent);y+=d(20);
        heading(L"Updates");row(L"Automatische Updates",automatic?L"Aktiviert":L"Deaktiviert",Automatic);row(L"Auf Updates prüfen",L"Jetzt prüfen",CheckUpdate);
        text(dc,updateText,rect(left+d(20),y,width-d(40),d(65)),15,false,secondary(),L"Segoe UI",DT_LEFT|DT_WORDBREAK);y+=d(72);
        if(updater.snapshot().phase==UpdatePhase::Ready){row(L"Neue Version installieren",L"Jetzt installieren",InstallUpdate);}
        row(L"PAF-Videos "+std::wstring(PAF_VERSION_W)+L" · Native C++ / libVLC",L"GitHub-Releases",Releases);RestoreDC(dc,old);
    }
    std::vector<fs::path> chooseFiles(bool folder=false,bool subtitleOnly=false){
        std::vector<fs::path> paths;IFileOpenDialog* dialog=nullptr;
        if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog))))return paths;
        DWORD options=0;dialog->GetOptions(&options);dialog->SetOptions(options|FOS_FORCEFILESYSTEM|(folder?FOS_PICKFOLDERS:FOS_ALLOWMULTISELECT));
        COMDLG_FILTERSPEC filterSpecs[]={{L"Alle Dateien",L"*.*"},{L"Untertitel",L"*.srt;*.ass;*.ssa;*.vtt"}};
        if(!folder){dialog->SetFileTypes(subtitleOnly?1:2,subtitleOnly?filterSpecs+1:filterSpecs);}
        dialog->SetTitle(folder?L"Medienordner auswählen":subtitleOnly?L"Untertitel öffnen":L"Medien öffnen");
        if(SUCCEEDED(dialog->Show(window))){IShellItemArray* items=nullptr;if(SUCCEEDED(dialog->GetResults(&items))){DWORD count=0;items->GetCount(&count);
            for(DWORD i=0;i<count;++i){IShellItem* item=nullptr;PWSTR path=nullptr;if(SUCCEEDED(items->GetItemAt(i,&item))){if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){paths.emplace_back(path);CoTaskMemFree(path);}item->Release();}}items->Release();}}
        dialog->Release();return paths;
    }
    void enqueue(const std::vector<fs::path>& paths){
        auto start=queue.size();for(const auto& path:paths){std::error_code error;if(fs::is_regular_file(path,error))queue.push_back(path);}
        if(queue.size()>start){playAt(int(start));refreshList();}
    }
    void playAt(int next){
        if(next<0||size_t(next)>=queue.size())return;
        index=next;page=Playback;layout();if(!player.open(queue[size_t(index)])){MessageBoxW(window,L"Die Datei konnte nicht geöffnet werden.",L"PAF-Videos",MB_ICONWARNING);return;}
        const float rates[]={.5f,.75f,1.f,1.25f,1.5f,2.f};player.rate(rates[speedIndex]);player.volume(volumeValue);player.mute(muted);
        if(remember&&!test){recent.erase(std::remove(recent.begin(),recent.end(),queue[size_t(index)]),recent.end());recent.insert(recent.begin(),queue[size_t(index)]);if(recent.size()>20)recent.resize(20);writePaths(data/L"recent.m3u8",recent);}
        lastState=0;SetWindowTextW(window,(L"PAF-Videos — "+queue[size_t(index)].filename().wstring()).c_str());
    }
    void step(int direction){
        if(queue.empty())return;int next=index+direction;
        if(shuffle)next=int(random()%queue.size());else if(repeat)next=(next+int(queue.size()))%int(queue.size());
        if(next>=0&&size_t(next)<queue.size())playAt(next);else player.stop();
    }
    void show(Page value){page=value;settingsScroll=0;refreshList();layout();}
    void refreshList(){
        listPaths=page==Home?recent:page==Music?musicLibrary:page==Video?videoLibrary:page==Queue?queue:page==Playlists?playlistPaths:std::vector<fs::path>{};
        displayed.clear();SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_RESETCONTENT,0,0);
        auto lower=[](std::wstring text){std::transform(text.begin(),text.end(),text.begin(),[](wchar_t c){return wchar_t(towlower(c));});return text;};auto query=lower(filter);
        for(size_t i=0;i<listPaths.size();++i){auto name=listPaths[i].filename().wstring();if(lower(name).find(query)==std::wstring::npos)continue;displayed.push_back(int(i));SendMessageW(list,LB_ADDSTRING,0,(LPARAM)name.c_str());}
        SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,TRUE);
    }
    void selectList(){
        int item=int(SendMessageW(list,LB_GETCURSEL,0,0));if(item<0||size_t(item)>=displayed.size())return;int selected=displayed[size_t(item)];
        if(page==Queue)playAt(selected);else if(page==Playlists){queue=playlistPaths;playAt(selected);}else enqueue({listPaths[size_t(selected)]});
    }
    void loadPlaylists(){
        playlistFiles.clear();SendMessageW(playlistCombo,CB_RESETCONTENT,0,0);std::error_code error;
        for(const auto& entry:fs::directory_iterator(data/L"playlists",error))if(entry.is_regular_file()&&entry.path().extension()==L".m3u8")playlistFiles.push_back(entry.path());
        std::sort(playlistFiles.begin(),playlistFiles.end());for(const auto& path:playlistFiles)SendMessageW(playlistCombo,CB_ADDSTRING,0,(LPARAM)path.stem().c_str());
        if(!playlistFiles.empty())SendMessageW(playlistCombo,CB_SETCURSEL,0,0);changePlaylist();
    }
    void changePlaylist(){int selected=int(SendMessageW(playlistCombo,CB_GETCURSEL,0,0));playlistPaths=selected>=0&&size_t(selected)<playlistFiles.size()?readPaths(playlistFiles[size_t(selected)]):std::vector<fs::path>{};if(page==Playlists)refreshList();}
    void savePlaylist(){
        if(queue.empty()){MessageBoxW(window,L"Die Warteschlange ist leer.",L"PAF-Videos",MB_OK);return;}
        IFileSaveDialog* dialog=nullptr;if(FAILED(CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog))))return;
        COMDLG_FILTERSPEC spec={L"M3U8 Wiedergabeliste",L"*.m3u8"};dialog->SetFileTypes(1,&spec);dialog->SetDefaultExtension(L"m3u8");dialog->SetFileName(L"Meine Wiedergabeliste.m3u8");dialog->SetTitle(L"Wiedergabeliste speichern");
        IShellItem* folder=nullptr;if(SUCCEEDED(SHCreateItemFromParsingName((data/L"playlists").c_str(),nullptr,IID_PPV_ARGS(&folder)))){dialog->SetFolder(folder);folder->Release();}
        if(SUCCEEDED(dialog->Show(window))){IShellItem* item=nullptr;PWSTR path=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){fs::path destination(path);CoTaskMemFree(path);writePaths(destination,queue);auto local=data/L"playlists"/destination.filename();if(local!=destination)writePaths(local,queue);}item->Release();}}
        dialog->Release();loadPlaylists();
    }
    void scanLibrary(bool audio){
        bool& running=audio?scanningMusic:scanningVideo;if(running)return;running=true;auto folders=audio?musicFolders:videoFolders;HWND target=window;
        workers.emplace_back([audio,folders,target]{auto result=std::make_unique<LibraryResult>();result->audio=audio;result->paths=paf::scan(folders,audio);if(PostMessageW(target,WM_LIBRARY,0,(LPARAM)result.get()))result.release();});
        InvalidateRect(window,nullptr,FALSE);
    }
    void manageFolders(){
        HMENU menu=CreatePopupMenu();UINT cmd=500;std::vector<std::pair<bool,size_t>> entries;
        for(bool audio:{true,false}){auto& folders=audio?musicFolders:videoFolders;for(size_t i=0;i<folders.size();++i){AppendMenuW(menu,MF_STRING,cmd++,(std::wstring(audio?L"Musik: ":L"Video: ")+folders[i].wstring()).c_str());entries.emplace_back(audio,i);}}
        if(entries.empty()){DestroyMenu(menu);MessageBoxW(window,L"Es sind noch keine Bibliotheksordner hinzugefügt.",L"PAF-Videos",MB_OK);return;}
        POINT p{};GetCursorPos(&p);int choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,window,nullptr);DestroyMenu(menu);
        if(choice>=500&&size_t(choice-500)<entries.size()&&MessageBoxW(window,L"Diesen Ordner aus der Bibliothek entfernen? Dateien bleiben erhalten.",L"PAF-Videos",MB_YESNO|MB_ICONQUESTION)==IDYES){
            auto [audio,index]=entries[size_t(choice-500)];auto& folders=audio?musicFolders:videoFolders;folders.erase(folders.begin()+ptrdiff_t(index));writePaths(data/(audio?L"music-folders.txt":L"video-folders.txt"),folders);scanLibrary(audio);
        }
    }
    void toggleFullScreen(){
        if(!fullscreen){restoreStyle=DWORD(GetWindowLongPtrW(window,GWL_STYLE));GetWindowRect(window,&restoreRect);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&mi);
            SetWindowLongPtrW(window,GWL_STYLE,restoreStyle&~DWORD(WS_OVERLAPPEDWINDOW));SetWindowPos(window,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED);
        }else{SetWindowLongPtrW(window,GWL_STYLE,restoreStyle);SetWindowPos(window,nullptr,restoreRect.left,restoreRect.top,restoreRect.right-restoreRect.left,restoreRect.bottom-restoreRect.top,SWP_FRAMECHANGED|SWP_NOZORDER);}
        fullscreen=!fullscreen;layout();
    }
    void update(){updater.start(window,WM_UPDATE,PAF_VERSION,data/L"updates",installedApp(directory));}
    void applyUpdate(bool restart){
        auto state=updater.snapshot();if(state.phase!=UpdatePhase::Ready)return;
        if(!startUpdateHelper(directory/L"PAF-Videos.Update.exe",state.installer,directory,state.sha256,restart)){
            MessageBoxW(window,L"Das Update konnte nicht gestartet werden. Bitte Setup erneut herunterladen und ausführen.",L"PAF-Videos",MB_ICONERROR);return;
        }
        automatic=false;PostMessageW(window,WM_CLOSE,0,0);
    }
    void command(int cmd){
        switch(cmd){
        case NavToggle:expanded=!expanded;save(L"expanded",expanded);layout();break;
        case OpenFiles:enqueue(chooseFiles());break;
        case OpenFolder:{auto folders=chooseFiles(true);auto paths=paf::scan(folders,false),audio=paf::scan(folders,true);paths.insert(paths.end(),audio.begin(),audio.end());enqueue(paths);break;}
        case ShowHome:case ShowMusic:case ShowVideo:case ShowQueue:case ShowPlaylists:case ShowSettings:case ShowPlayback:show(Page(cmd-ShowHome));break;
        case PlayPause:if(index<0){if(queue.empty())enqueue(chooseFiles());else playAt(0);}else if(player.state()==5||player.state()==6||player.state()==7)playAt(index);else player.pause();break;
        case Previous:step(-1);break;case Next:step(1);break;
        case Shuffle:shuffle=!shuffle;break;case Repeat:repeat=!repeat;break;
        case Mute:muted=!muted;player.mute(muted);break;case FullScreen:toggleFullScreen();break;
        case Subtitle:{auto paths=chooseFiles(false,true);if(!paths.empty()){wchar_t uri[32768]{};DWORD size=32768;if(SUCCEEDED(UrlCreateFromPathW(paths[0].c_str(),uri,&size,0))&&!player.subtitle(uri))MessageBoxW(window,L"Untertitel konnten nicht geladen werden.",L"PAF-Videos",MB_ICONWARNING);}break;}
        case AudioTrack:{auto tracks=player.tracks();auto menu=CreatePopupMenu();for(size_t i=0;i<tracks.size();++i)AppendMenuW(menu,MF_STRING,600+UINT(i),tracks[i].second.c_str());POINT p{};GetCursorPos(&p);int choice=TrackPopupMenu(menu,TPM_RETURNCMD,p.x,p.y,0,window,nullptr);DestroyMenu(menu);if(choice>=600&&size_t(choice-600)<tracks.size())player.audioTrack(tracks[size_t(choice-600)].first);break;}
        case Speed:{speedIndex=(speedIndex+1)%6;const float values[]={.5f,.75f,1,1.25f,1.5f,2};player.rate(values[speedIndex]);break;}
        case ClearQueue:player.stop();queue.clear();index=-1;SetWindowTextW(window,L"PAF-Videos");refreshList();break;
        case SavePlaylist:savePlaylist();break;
        case DeletePlaylist:{int selection=int(SendMessageW(playlistCombo,CB_GETCURSEL,0,0));if(selection>=0&&size_t(selection)<playlistFiles.size()&&MessageBoxW(window,L"Diese Wiedergabeliste löschen?",L"PAF-Videos",MB_YESNO)==IDYES){fs::remove(playlistFiles[size_t(selection)]);loadPlaylists();}break;}
        case AddMusic:case AddVideo:{bool audio=cmd==AddMusic;auto selected=chooseFiles(true);auto& folders=audio?musicFolders:videoFolders;for(auto& folder:selected)if(std::find(folders.begin(),folders.end(),folder)==folders.end())folders.push_back(folder);writePaths(data/(audio?L"music-folders.txt":L"video-folders.txt"),folders);scanLibrary(audio);break;}
        case ManageMusic:case ManageVideo:manageFolders();break;case Refresh:scanLibrary(true);scanLibrary(false);break;
        case Theme:theme=(theme+1)%3;save(L"theme",theme);layout();break;case Accent:accent=(accent+1)%4;save(L"accent",accent);break;
        case Remember:remember=!remember;save(L"remember",remember);if(!remember){recent.clear();writePaths(data/L"recent.m3u8",recent);}break;
        case ClearRecent:recent.clear();writePaths(data/L"recent.m3u8",recent);if(page==Home)refreshList();break;
        case Automatic:automatic=!automatic;save(L"automatic",automatic);break;case CheckUpdate:update();break;
        case InstallUpdate:applyUpdate(true);break;case Releases:ShellExecuteW(window,L"open",L"https://github.com/Lyvt-Dev/PAF-Videos/releases",nullptr,nullptr,SW_SHOWNORMAL);break;
        }
        InvalidateRect(window,nullptr,FALSE);
    }
    void onUpdate(){
        auto snapshot=updater.snapshot();switch(snapshot.phase){
        case UpdatePhase::Checking:updateText=L"Suche nach Updates …";break;case UpdatePhase::Current:updateText=L"PAF-Videos ist aktuell.";break;
        case UpdatePhase::Available:updateText=L"Version "+wide(snapshot.version)+L" verfügbar. Portable Apps: GitHub-Releases öffnen.";break;
        case UpdatePhase::Downloading:updateText=L"Version "+wide(snapshot.version)+L" wird heruntergeladen …";break;
        case UpdatePhase::Ready:updateText=L"Version "+wide(snapshot.version)+L" bereit. Installation beim Beenden oder über Jetzt installieren.";break;
        case UpdatePhase::Failed:updateText=L"Update-Prüfung fehlgeschlagen. Offline-Wiedergabe bleibt verfügbar; später erneut prüfen.";break;default:break;
        }InvalidateRect(window,nullptr,FALSE);
    }
    void tick(){
        int state=player.state();if(!dragSeek)SendMessageW(seek,TBM_SETPOS,TRUE,int(std::max<int64_t>(0,player.time())*10000/std::max<int64_t>(1,player.length())));
        RECT controls=client();controls.top=controlsTop();InvalidateRect(window,&controls,FALSE);
        if(!test&&state==6&&lastState!=6)step(1);
        if(!test&&state==7&&lastState!=7){player.stop();MessageBoxW(window,L"Die Datei ist beschädigt, geschützt oder der Codec wird nicht unterstützt.",L"PAF-Videos",MB_ICONWARNING);}lastState=state;
        auto now=GetTickCount64();if(automatic&&now>=nextUpdate){nextUpdate=now+24ull*60*60*1000;update();}
        if(normalProbe&&now-started>=6500){
            bool ok=IsWindowVisible(window)&&IsWindow(window)&&player.state()!=7;std::ofstream out(report);out<<(ok?"PASS":"FAIL")<<" normal launch survived event loop and automatic update startup\n";
            normalProbe=false;PostMessageW(window,WM_CLOSE,0,0);if(!ok)selfStage=-1;
        }
        if(test){auto stats=player.stats();if(stats.decodedVideo>0){
            bool ok=IsWindowVisible(window)&&logo&&logo->GetWidth()>0;std::ofstream out(report);out<<(ok?"PASS":"FAIL")<<" native HWND, embedded logo, bundled VLC, "<<stats.decodedVideo<<" decoded video frames\n";
            selfStage=ok?1:-1;PostMessageW(window,WM_CLOSE,0,0);
        }else if(now-started>15000){std::ofstream(report)<<"FAIL VLC did not decode a video frame, state="<<state<<"\n";selfStage=-1;PostMessageW(window,WM_CLOSE,0,0);}}
    }
    void shutdown(){
        if(closing)return;closing=true;KillTimer(window,1);updater.stop();
        if(automatic&&updater.snapshot().phase==UpdatePhase::Ready){auto state=updater.snapshot();startUpdateHelper(directory/L"PAF-Videos.Update.exe",state.installer,directory,state.sha256,false);}
        for(auto& worker:workers)if(worker.joinable())worker.join();
        MSG message{};while(PeekMessageW(&message,window,WM_LIBRARY,WM_LIBRARY,PM_REMOVE))delete reinterpret_cast<LibraryResult*>(message.lParam);
        player.close();if(controlFont){DeleteObject(controlFont);controlFont=nullptr;}
    }
    ~App(){if(window&&!closing)shutdown();}
};
void startupError(const fs::path& data,const std::wstring& message,bool silent=false){
    try{fs::create_directories(data);std::ofstream out(data/L"startup.log",std::ios::app);out<<"PAF-Videos "<<PAF_VERSION<<": "<<utf8(message)<<"\n";}catch(...){}
    if(!silent)MessageBoxW(nullptr,(message+L"\n\nStartprotokoll: "+(data/L"startup.log").wstring()).c_str(),L"PAF-Videos – Start fehlgeschlagen",MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
}
LRESULT CALLBACK windowProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
    auto app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){auto create=reinterpret_cast<CREATESTRUCTW*>(lp);app=static_cast<App*>(create->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
    if(!app)return DefWindowProcW(hwnd,message,wp,lp);
    try{switch(message){
    case WM_CREATE:app->create(hwnd);return 0;
    case WM_SIZE:if(app->surface)app->layout();return 0;
    case WM_DPICHANGED:{app->scale=HIWORD(wp)/96.f;auto r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);if(app->surface)app->layout();return 0;}
    case WM_GETMINMAXINFO:{auto info=reinterpret_cast<MINMAXINFO*>(lp);info->ptMinTrackSize={app->d(860),app->d(680)};return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT paint{};auto dc=BeginPaint(hwnd,&paint);if(app->logo)app->paint(dc);EndPaint(hwnd,&paint);return 0;}
    case WM_LBUTTONUP:{POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        for(auto& button:app->buttons)if(PtInRect(&button.rect,point)){
            if(app->page==Settings&&button.rect.top<app->d(145)&&button.rect.left>=app->contentLeft())continue;
            if(app->page==Settings&&button.rect.top<app->controlsTop()&&button.rect.bottom>app->controlsTop()&&button.rect.left>=app->contentLeft())continue;
            int cmd=button.command;app->command(cmd);break;
        }return 0;}
    case WM_MOUSEWHEEL:if(app->page==Settings){app->settingsScroll=std::clamp(app->settingsScroll-GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA*app->d(90),0,std::max(0,app->d(1510)-app->controlsTop()));InvalidateRect(hwnd,nullptr,FALSE);return 0;}break;
    case WM_COMMAND:
        if(LOWORD(wp)==300&&HIWORD(wp)==LBN_DBLCLK){app->selectList();return 0;}
        if(LOWORD(wp)==301&&HIWORD(wp)==EN_CHANGE){int count=GetWindowTextLengthW(app->search);std::wstring text(size_t(count)+1,L'\0');GetWindowTextW(app->search,text.data(),count+1);text.resize(size_t(count));app->filter=text;app->refreshList();return 0;}
        if(LOWORD(wp)==302&&HIWORD(wp)==CBN_SELCHANGE){app->changePlaylist();return 0;}
        app->command(LOWORD(wp));return 0;
    case WM_HSCROLL:
        if(reinterpret_cast<HWND>(lp)==app->volume){app->volumeValue=int(SendMessageW(app->volume,TBM_GETPOS,0,0));app->player.volume(app->volumeValue);app->save(L"volume",app->volumeValue);}
        else if(reinterpret_cast<HWND>(lp)==app->seek){app->dragSeek=LOWORD(wp)==TB_THUMBTRACK;if(LOWORD(wp)!=TB_THUMBTRACK)app->player.seek(SendMessageW(app->seek,TBM_GETPOS,0,0)*std::max<int64_t>(0,app->player.length())/10000);}
        return 0;
    case WM_DRAWITEM:{auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(item->CtlID==300){
        app->fill(item->hDC,item->rcItem,item->itemState&ODS_SELECTED?app->orange():app->card());
        if(item->itemID!=UINT(-1)&&item->itemID<app->displayed.size()){auto index=app->displayed[item->itemID];auto r=item->rcItem;r.left+=app->d(18);app->text(item->hDC,app->listPaths[size_t(index)].filename().wstring(),r,17,false,item->itemState&ODS_SELECTED?RGB(20,20,20):app->fg());}return TRUE;
        }break;}
    case WM_CTLCOLORLISTBOX:case WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:{static HBRUSH brush=nullptr;static COLORREF color=CLR_INVALID;
        if(color!=app->card()){if(brush)DeleteObject(brush);color=app->card();brush=CreateSolidBrush(color);}SetTextColor((HDC)wp,app->fg());SetBkColor((HDC)wp,color);return (LRESULT)brush;}
    case WM_DROPFILES:{auto drop=reinterpret_cast<HDROP>(wp);UINT count=DragQueryFileW(drop,UINT(-1),nullptr,0);std::vector<fs::path> paths;
        for(UINT i=0;i<count;++i){UINT length=DragQueryFileW(drop,i,nullptr,0);std::wstring value(length+1,L'\0');DragQueryFileW(drop,i,value.data(),length+1);value.resize(length);paths.emplace_back(value);}DragFinish(drop);app->enqueue(paths);return 0;}
    case WM_KEYDOWN:if(wp==VK_ESCAPE&&app->fullscreen){app->toggleFullScreen();return 0;}if(wp==VK_RIGHT){app->player.seek(app->player.time()+10000);return 0;}if(wp==VK_LEFT){app->player.seek(app->player.time()-10000);return 0;}break;
    case WM_TIMER:app->tick();return 0;
    case WM_UPDATE:app->onUpdate();return 0;
    case WM_LIBRARY:{std::unique_ptr<LibraryResult> result(reinterpret_cast<LibraryResult*>(lp));if(result->audio){app->musicLibrary=std::move(result->paths);app->scanningMusic=false;}else{app->videoLibrary=std::move(result->paths);app->scanningVideo=false;}app->refreshList();InvalidateRect(hwnd,nullptr,FALSE);return 0;}
    case WM_CLOSE:app->shutdown();DestroyWindow(hwnd);return 0;
    case WM_DESTROY:PostQuitMessage(app->selfStage<0?1:0);return 0;
    }}catch(const std::exception& error){
        auto description=wide(error.what());startupError(app->data,description,app->test||app->normalProbe);
        if(!app->report.empty())std::ofstream(app->report)<<"FAIL "<<error.what()<<'\n';
        app->selfStage=-1;
        if(message==WM_CREATE)return -1;PostMessageW(hwnd,WM_CLOSE,0,0);return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);std::vector<std::wstring> arguments;for(int i=1;args&&i<count;++i)arguments.emplace_back(args[i]);if(args)LocalFree(args);
    if(arguments.size()==4&&arguments[0]==L"--test-update"){
        auto directory=executableDirectory();std::string hash=utf8(arguments[2]);
        return startUpdateHelper(directory/L"PAF-Videos.Update.exe",arguments[1],directory,hash,false)?0:1;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);GdiplusStartupInput input;ULONG_PTR gdiplus=0;
    if(GdiplusStartup(&gdiplus,&input,nullptr)!=Ok)return 1;
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
    int exitCode=1;
    {
        App app;
        if(arguments.size()==3&&arguments[0]==L"--self-test"){app.test=true;app.fixture=arguments[1];app.report=arguments[2];}
        if(arguments.size()==2&&arguments[0]==L"--normal-start-test"){app.normalProbe=true;app.report=arguments[1];}
        try{
            app.initializeData();std::ofstream(app.data/L"startup.log",std::ios::app)<<"Starting native PAF-Videos "<<PAF_VERSION<<'\n';
            WNDCLASSEXW cls{sizeof(cls)};cls.style=CS_HREDRAW|CS_VREDRAW;cls.lpfnWndProc=windowProc;cls.hInstance=instance;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));cls.hIconSm=cls.hIcon;cls.lpszClassName=L"PAFVideos.Native";
            if(!RegisterClassExW(&cls))throw std::runtime_error("Cannot register native window class");
            auto hwnd=CreateWindowExW(0,cls.lpszClassName,L"PAF-Videos",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1280,960,nullptr,nullptr,instance,&app);
            if(!hwnd)throw std::runtime_error("Cannot create the PAF-Videos window; see startup.log for details.");
            ShowWindow(hwnd,show==0?SW_SHOWNORMAL:show);UpdateWindow(hwnd);
            if(app.test)app.enqueue({app.fixture});else if(!app.normalProbe&&!arguments.empty()){std::vector<fs::path> paths;for(auto& arg:arguments)paths.emplace_back(arg);app.enqueue(paths);}
            ACCEL shortcuts[]={{FVIRTKEY|FCONTROL,'O',OpenFiles},{FVIRTKEY,VK_SPACE,PlayPause},{FVIRTKEY,VK_F11,FullScreen}};
            auto accelerators=CreateAcceleratorTableW(shortcuts,3);MSG message{};int result=0;
            while((result=GetMessageW(&message,nullptr,0,0))>0){if(!TranslateAcceleratorW(hwnd,accelerators,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
            if(accelerators)DestroyAcceleratorTable(accelerators);exitCode=result<0?1:int(message.wParam);
        }catch(const std::exception& error){startupError(app.data.empty()?fs::temp_directory_path()/L"PAF-Videos":app.data,wide(error.what()),app.test||app.normalProbe);if(!app.report.empty())std::ofstream(app.report)<<"FAIL "<<error.what()<<'\n';}
    }
    GdiplusShutdown(gdiplus);if(SUCCEEDED(com))CoUninitialize();return exitCode;
}
