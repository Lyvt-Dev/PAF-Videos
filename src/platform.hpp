#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
#include "core.hpp"
namespace fs=std::filesystem;
inline std::wstring wide(const std::string& value){
    if(value.empty())return {};int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
    std::wstring result(count,L'\0');if(count)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),result.data(),count);return result;
}
inline std::string utf8(const std::wstring& value){
    if(value.empty())return {};int count=WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);
    std::string result(count,'\0');if(count)WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),result.data(),count,nullptr,nullptr);return result;
}
inline fs::path executableDirectory(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);return fs::path(path).parent_path();}
