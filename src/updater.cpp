#include "updater.hpp"
#include "platform.hpp"
#include <winhttp.h>
#include <bcrypt.h>
#include <fstream>
#include <array>
struct InternetHandle {HINTERNET value=nullptr;~InternetHandle(){if(value)WinHttpCloseHandle(value);}};
std::string updateHash(const fs::path& file){
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    DWORD size=0,bytes=0;BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&bytes,0);
    std::vector<BYTE> object(size);std::array<BYTE,32> digest{};bool valid=BCryptCreateHash(algorithm,&hash,object.data(),size,nullptr,0,0)>=0;
    std::ifstream input(file,std::ios::binary);std::array<char,65536> buffer{};valid&=bool(input);
    while(valid&&input){input.read(buffer.data(),buffer.size());auto count=input.gcount();if(count)valid=BCryptHashData(hash,reinterpret_cast<PUCHAR>(buffer.data()),ULONG(count),0)>=0;}
    valid&=input.eof();if(valid)valid=BCryptFinishHash(hash,digest.data(),DWORD(digest.size()),0)>=0;
    if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
    if(!valid)return {};const char* hex="0123456789abcdef";std::string output;for(auto byte:digest){output+=hex[byte>>4];output+=hex[byte&15];}return output;
}
bool installedApp(const fs::path& directory){
    wchar_t path[32768]{};DWORD bytes=sizeof(path);
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\PAFVideos",L"InstallDir",RRF_RT_REG_SZ,nullptr,path,&bytes)!=ERROR_SUCCESS)return false;
    std::error_code error;return fs::equivalent(directory,fs::path(path),error)&&fs::is_regular_file(directory/L"unins000.exe",error);
}
UpdateSnapshot UpdateService::snapshot(){std::lock_guard<std::mutex> lock(mutex);return state;}
void UpdateService::publish(UpdateSnapshot next){
    {std::lock_guard<std::mutex> lock(mutex);state=std::move(next);}
    if(!cancelled)PostMessageW(target,message,0,0);
}
bool UpdateService::fetch(const std::wstring& url,const fs::path& path,size_t maximum,std::string* text){
    URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=DWORD(-1);parts.dwUrlPathLength=DWORD(-1);
    if(!WinHttpCrackUrl(url.c_str(),0,0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS)return false;
    std::wstring host(parts.lpszHostName,parts.dwHostNameLength),endpoint(parts.lpszUrlPath,parts.dwUrlPathLength);
    InternetHandle session{WinHttpOpen(L"PAF-Videos Updater",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.value)return false;WinHttpSetTimeouts(session.value,5000,5000,5000,5000);
    InternetHandle connection{WinHttpConnect(session.value,host.c_str(),INTERNET_DEFAULT_HTTPS_PORT,0)};if(!connection.value)return false;
    auto request=WinHttpOpenRequest(connection.value,L"GET",endpoint.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);if(!request)return false;
    {std::lock_guard<std::mutex> lock(httpMutex);if(cancelled){WinHttpCloseHandle(request);return false;}activeRequest=request;}
    auto close=[&]{std::lock_guard<std::mutex> lock(httpMutex);if(activeRequest==request){activeRequest=nullptr;WinHttpCloseHandle(request);}};
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
    bool ok=WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)&&WinHttpReceiveResponse(request,nullptr);
    DWORD status=0,bytes=sizeof(status);ok=ok&&WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&bytes,WINHTTP_NO_HEADER_INDEX)&&status==200;
    std::ofstream output;if(!text){output.open(path,std::ios::binary|std::ios::trunc);ok=ok&&bool(output);}
    size_t total=0;std::array<char,65536> buffer{};
    while(ok&&!cancelled){DWORD count=0;if(!WinHttpReadData(request,buffer.data(),DWORD(buffer.size()),&count)){ok=false;break;}if(!count)break;
        total+=count;if(total>maximum){ok=false;break;}if(text)text->append(buffer.data(),count);else{output.write(buffer.data(),count);ok=bool(output);}}
    if(output.is_open()){output.flush();ok=ok&&bool(output);output.close();}close();return ok&&!cancelled;
}
bool UpdateService::start(HWND window,UINT notification,const std::string& current,const fs::path& cache,bool download){
    if(running.exchange(true))return false;if(worker.joinable())worker.join();cancelled=false;target=window;message=notification;
    publish({UpdatePhase::Checking});
    worker=std::thread([this,current,cache,download]{
        try{
            std::string text;
            if(!fetch(L"https://github.com/Lyvt-Dev/PAF-Videos/releases/latest/download/update.json",{},8192,&text))throw 1;
            auto value=paf::manifest(text);if(!value)throw 1;
            if(!paf::newer(value->version,current)){publish({UpdatePhase::Current});running=false;return;}
            UpdateSnapshot result{UpdatePhase::Available,value->version,value->sha256,{}};
            publish(result);if(!download){running=false;return;}
            result.phase=UpdatePhase::Downloading;publish(result);std::error_code error;fs::create_directories(cache,error);if(error)throw 1;
            std::wstring version(value->version.begin(),value->version.end());auto destination=cache/(L"setup-"+version+L".exe");
            bool cached=fs::is_regular_file(destination,error)&&fs::file_size(destination,error)==value->size&&updateHash(destination)==value->sha256;
            if(!cached){auto part=fs::path(destination.wstring()+L".part");
                bool received=fetch(L"https://github.com/Lyvt-Dev/PAF-Videos/releases/download/v"+version+L"/setup.exe",part,size_t(value->size));
                bool valid=received&&fs::file_size(part,error)==value->size&&updateHash(part)==value->sha256;
                if(!valid){fs::remove(part,error);throw 1;}if(!MoveFileExW(part.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw 1;
            }
            result.phase=UpdatePhase::Ready;result.installer=destination;publish(result);
        }catch(...){if(!cancelled)publish({UpdatePhase::Failed});}
        running=false;
    });return true;
}
void UpdateService::stop(){cancelled=true;{std::lock_guard<std::mutex> lock(httpMutex);if(activeRequest){WinHttpCloseHandle(activeRequest);activeRequest=nullptr;}}if(worker.joinable())worker.join();running=false;}
bool startUpdateHelper(const fs::path& helper,const fs::path& installer,const fs::path& directory,const std::string& checksum,bool restart){
    if(checksum.size()!=64||updateHash(installer)!=checksum||!installedApp(directory))return false;
    // Run outside the installation folder so setup can replace its own helper.
    auto staged=installer.parent_path()/(L"apply-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".exe");
    std::error_code error;if(!fs::copy_file(helper,staged,fs::copy_options::none,error)||updateHash(helper)!=updateHash(staged))return false;
    std::wstring hash(checksum.begin(),checksum.end());
    auto arguments=paf::quote(staged.wstring())+L" --apply "+std::to_wstring(GetCurrentProcessId())+L" "+paf::quote(installer.wstring())+L" "+paf::quote(directory.wstring())+L" "+hash+(restart?L" 1":L" 0");
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(staged.c_str(),arguments.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,directory.c_str(),&startup,&process))return false;
    CloseHandle(process.hThread);CloseHandle(process.hProcess);return true;
}
