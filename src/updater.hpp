#pragma once
#include "platform.hpp"
#include "update_core.hpp"
#include <thread>
#include <mutex>
#include <atomic>
enum class UpdatePhase {Idle,Checking,Current,Available,Downloading,Ready,Failed};
struct UpdateSnapshot {
    UpdatePhase phase=UpdatePhase::Idle;
    std::string version,sha256;
    fs::path installer;
};
class UpdateService {
    std::mutex mutex,httpMutex;
    UpdateSnapshot state;
    std::thread worker;
    std::atomic<bool> cancelled{false},running{false};
    void* activeRequest=nullptr;
    HWND target=nullptr;
    UINT message=0;
    bool fetch(const std::wstring& url,const fs::path& path,size_t maximum,std::string* text=nullptr);
    void publish(UpdateSnapshot next);
public:
    ~UpdateService(){stop();}
    void prepareDownload(){stop();cancelled=false;}
    bool download(const std::wstring& url,const fs::path& path,size_t maximum,std::string* text=nullptr){return fetch(url,path,maximum,text);}
    UpdateSnapshot snapshot();
    bool start(HWND window,UINT notification,const std::string& current,const fs::path& cache,bool download);
    void stop();
};
std::string updateHash(const fs::path& file);
bool installedApp(const fs::path& directory);
bool startUpdateHelper(const fs::path& helper,const fs::path& installer,const fs::path& directory,
                       const std::string& checksum,bool restart);
