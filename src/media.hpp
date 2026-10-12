#pragma once
#include "platform.hpp"
#include <stdexcept>
struct libvlc_instance_t;
struct libvlc_media_t;
struct libvlc_media_player_t;
struct TrackDescription {int id;char* name;TrackDescription* next;};
struct MediaStats {
    int readBytes;float inputBitrate;int demuxReadBytes;float demuxBitrate;
    int demuxCorrupted,demuxDiscontinuity,decodedVideo,decodedAudio,displayedPictures,lostPictures,playedAudio,lostAudio,sentPackets,sentBytes;float sendBitrate;
};
class MediaPlayer {
    HMODULE module=nullptr;DLL_DIRECTORY_COOKIE cookie=nullptr;libvlc_instance_t* instance=nullptr;
    libvlc_media_player_t* player=nullptr;libvlc_media_t* media=nullptr;
    template<class T>T symbol(const char* name){auto proc=GetProcAddress(module,name);if(!proc)throw std::runtime_error(std::string("Missing VLC function: ")+name);return reinterpret_cast<T>(proc);}
#define VLC_FUNCTION(ret,name,...) using name##_type=ret(__cdecl*)(__VA_ARGS__);name##_type name=nullptr
    VLC_FUNCTION(libvlc_instance_t*,libvlc_new,int,const char* const*);
    VLC_FUNCTION(void,libvlc_release,libvlc_instance_t*);
    VLC_FUNCTION(libvlc_media_player_t*,libvlc_media_player_new,libvlc_instance_t*);
    VLC_FUNCTION(void,libvlc_media_player_release,libvlc_media_player_t*);
    VLC_FUNCTION(libvlc_media_t*,libvlc_media_new_path,libvlc_instance_t*,const char*);
    VLC_FUNCTION(void,libvlc_media_release,libvlc_media_t*);
    VLC_FUNCTION(void,libvlc_media_player_set_media,libvlc_media_player_t*,libvlc_media_t*);
    VLC_FUNCTION(void,libvlc_media_player_set_hwnd,libvlc_media_player_t*,void*);
    VLC_FUNCTION(int,libvlc_media_player_play,libvlc_media_player_t*);
    VLC_FUNCTION(void,libvlc_media_player_pause,libvlc_media_player_t*);
    VLC_FUNCTION(void,libvlc_media_player_stop,libvlc_media_player_t*);
    VLC_FUNCTION(int,libvlc_media_player_get_state,libvlc_media_player_t*);
    VLC_FUNCTION(int64_t,libvlc_media_player_get_time,libvlc_media_player_t*);
    VLC_FUNCTION(int64_t,libvlc_media_player_get_length,libvlc_media_player_t*);
    VLC_FUNCTION(void,libvlc_media_player_set_time,libvlc_media_player_t*,int64_t);
    VLC_FUNCTION(int,libvlc_audio_set_volume,libvlc_media_player_t*,int);
    VLC_FUNCTION(void,libvlc_audio_set_mute,libvlc_media_player_t*,int);
    VLC_FUNCTION(int,libvlc_media_player_set_rate,libvlc_media_player_t*,float);
    VLC_FUNCTION(int,libvlc_media_player_add_slave,libvlc_media_player_t*,int,const char*,bool);
    VLC_FUNCTION(TrackDescription*,libvlc_audio_get_track_description,libvlc_media_player_t*);
    VLC_FUNCTION(void,libvlc_track_description_list_release,TrackDescription*);
    VLC_FUNCTION(int,libvlc_audio_set_track,libvlc_media_player_t*,int);
    VLC_FUNCTION(int,libvlc_media_get_stats,libvlc_media_t*,MediaStats*);
#undef VLC_FUNCTION
public:
    MediaPlayer()=default;MediaPlayer(const MediaPlayer&)=delete;MediaPlayer& operator=(const MediaPlayer&)=delete;
    ~MediaPlayer(){close();}
    void close(){
        if(player){libvlc_media_player_stop(player);libvlc_media_player_release(player);player=nullptr;}
        if(media){libvlc_media_release(media);media=nullptr;}
        if(instance){libvlc_release(instance);instance=nullptr;}
        if(module){FreeLibrary(module);module=nullptr;}
        if(cookie){RemoveDllDirectory(cookie);cookie=nullptr;}
    }
    void initialize(HWND surface,bool test=false){
        const auto directory=executableDirectory()/L"vlc";
        if(!fs::is_regular_file(directory/L"libvlc.dll"))throw std::runtime_error("Bundled VLC is missing. Reinstall PAF-Videos or extract the entire portable ZIP.");
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        cookie=AddDllDirectory(directory.c_str());
        SetEnvironmentVariableW(L"VLC_PLUGIN_PATH",(directory/L"plugins").c_str());
        module=LoadLibraryExW((directory/L"libvlc.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!module)throw std::runtime_error("Cannot load bundled libvlc.dll (Windows error "+std::to_string(GetLastError())+").");
#define LOAD(name) name=symbol<name##_type>(#name)
        LOAD(libvlc_new);LOAD(libvlc_release);LOAD(libvlc_media_player_new);LOAD(libvlc_media_player_release);
        LOAD(libvlc_media_new_path);LOAD(libvlc_media_release);LOAD(libvlc_media_player_set_media);LOAD(libvlc_media_player_set_hwnd);
        LOAD(libvlc_media_player_play);LOAD(libvlc_media_player_pause);LOAD(libvlc_media_player_stop);LOAD(libvlc_media_player_get_state);
        LOAD(libvlc_media_player_get_time);LOAD(libvlc_media_player_get_length);LOAD(libvlc_media_player_set_time);
        LOAD(libvlc_audio_set_volume);LOAD(libvlc_audio_set_mute);LOAD(libvlc_media_player_set_rate);LOAD(libvlc_media_player_add_slave);
        LOAD(libvlc_audio_get_track_description);LOAD(libvlc_track_description_list_release);LOAD(libvlc_audio_set_track);LOAD(libvlc_media_get_stats);
#undef LOAD
        const char* args[]={"--no-video-title-show","--no-media-library","--avcodec-hw=none","--aout=dummy"};
        instance=libvlc_new(test?4:2,args);if(!instance)throw std::runtime_error("Bundled VLC plugins could not be initialized.");
        player=libvlc_media_player_new(instance);if(!player)throw std::runtime_error("Cannot create VLC player.");
        libvlc_media_player_set_hwnd(player,surface);
    }
    bool open(const fs::path& path){
        stop();if(media){libvlc_media_release(media);media=nullptr;}
        media=libvlc_media_new_path(instance,utf8(path.wstring()).c_str());if(!media)return false;
        libvlc_media_player_set_media(player,media);return libvlc_media_player_play(player)==0;
    }
    void stop(){if(player)libvlc_media_player_stop(player);}
    void pause(){if(player)libvlc_media_player_pause(player);}
    int state()const{return player?libvlc_media_player_get_state(player):0;}
    int64_t time()const{return player?libvlc_media_player_get_time(player):0;}
    int64_t length()const{return player?libvlc_media_player_get_length(player):0;}
    void seek(int64_t ms){if(player)libvlc_media_player_set_time(player,std::max<int64_t>(0,ms));}
    void volume(int value){if(player)libvlc_audio_set_volume(player,value);}
    void mute(bool value){if(player)libvlc_audio_set_mute(player,value);}
    void rate(float value){if(player)libvlc_media_player_set_rate(player,value);}
    bool subtitle(const std::wstring& uri){return player&&libvlc_media_player_add_slave(player,0,utf8(uri).c_str(),true)==0;}
    std::vector<std::pair<int,std::wstring>> tracks(){
        std::vector<std::pair<int,std::wstring>> result;if(!player)return result;
        auto head=libvlc_audio_get_track_description(player);for(auto it=head;it;it=it->next)result.emplace_back(it->id,wide(it->name?it->name:""));
        if(head)libvlc_track_description_list_release(head);return result;
    }
    void audioTrack(int id){if(player)libvlc_audio_set_track(player,id);}
    MediaStats stats()const{MediaStats value{};if(media)libvlc_media_get_stats(media,&value);return value;}
};
