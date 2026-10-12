#include "updater.hpp"
#include "platform.hpp"
#include <shellapi.h>
#include <fstream>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    int count=0;auto arguments=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!arguments||count!=7||std::wstring(arguments[1])!=L"--apply"){if(arguments)LocalFree(arguments);return 2;}
    DWORD parent=wcstoul(arguments[2],nullptr,10);fs::path setup(arguments[3]),directory(arguments[4]);
    std::wstring wideHash(arguments[5]);std::string checksum(wideHash.begin(),wideHash.end());bool restart=std::wstring(arguments[6])==L"1";LocalFree(arguments);
    if(!parent||!installedApp(directory)||checksum.size()!=64)return 3;
    HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,parent);
    if(process){auto result=WaitForSingleObject(process,300000);CloseHandle(process);if(result!=WAIT_OBJECT_0)return 4;}
    else if(GetLastError()!=ERROR_INVALID_PARAMETER)return 4;
    // Recheck after waiting: never run a corrupted or subsequently changed installer.
    if(updateHash(setup)!=checksum)return 5;
    auto command=paf::quote(setup.wstring())+L" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS /DIR="+paf::quote(directory.wstring());
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION installer{};
    if(!CreateProcessW(setup.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,directory.c_str(),&startup,&installer))return 6;
    CloseHandle(installer.hThread);WaitForSingleObject(installer.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(installer.hProcess,&code);CloseHandle(installer.hProcess);
    {
        auto resultPath=setup;resultPath.replace_extension(L".result.json");
        std::ofstream output(resultPath);output<<"{\"ok\":"<<(code?"false":"true")<<",\"installer_exit_code\":"<<code<<"}";
    }
    if(code)return int(code);
    if(restart){auto executable=directory/L"PAF-Videos.exe";auto launch=paf::quote(executable.wstring());PROCESS_INFORMATION app{};
        if(CreateProcessW(executable.c_str(),launch.data(),nullptr,nullptr,FALSE,0,nullptr,directory.c_str(),&startup,&app)){CloseHandle(app.hThread);CloseHandle(app.hProcess);}}
    return 0;
}
