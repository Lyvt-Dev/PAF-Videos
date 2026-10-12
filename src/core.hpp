#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>
namespace paf {
inline std::optional<std::array<unsigned,4>> version(const std::string& text){
    std::array<unsigned,4> result{};size_t cursor=0;
    for(size_t component=0;component<3;++component){
        size_t start=cursor;unsigned value=0;
        while(cursor<text.size()&&text[cursor]>='0'&&text[cursor]<='9'){
            if(cursor-start>=5)return {};value=value*10+unsigned(text[cursor++]-'0');if(value>65535)return {};
        }
        if(cursor==start||(cursor-start>1&&text[start]=='0'))return {};
        result[component]=value;
        if(component<2){if(cursor>=text.size()||text[cursor++]!='.')return {};}
    }
    if(text.substr(cursor)=="-fix"){result[3]=1;cursor=text.size();}
    if(cursor!=text.size())return {};return result;
}
inline bool newer(const std::string& candidate,const std::string& current){auto a=version(candidate),b=version(current);return a&&b&&*a>*b;}
struct Manifest {std::string version,sha256;uint64_t size=0;};
inline std::optional<Manifest> manifest(const std::string& text){
    if(text.size()>8192)return {};
    size_t cursor=0;Manifest out;std::set<std::string> keys;
    auto whitespace=[&]{while(cursor<text.size()&&std::isspace(static_cast<unsigned char>(text[cursor])))++cursor;};
    auto consume=[&](char c){whitespace();if(cursor==text.size()||text[cursor]!=c)return false;++cursor;return true;};
    auto string=[&]() -> std::optional<std::string>{
        if(!consume('"'))return {};auto start=cursor;
        while(cursor<text.size()&&text[cursor]!='"'){if(text[cursor]=='\\'||static_cast<unsigned char>(text[cursor])<32)return {};++cursor;}
        if(cursor==text.size())return {};auto value=text.substr(start,cursor-start);++cursor;return value;
    };
    if(!consume('{'))return {};
    for(int i=0;i<3;++i){
        auto key=string();if(!key||!keys.insert(*key).second||!consume(':'))return {};
        if(*key=="version"||*key=="sha256"){
            auto value=string();if(!value)return {};(*key=="version"?out.version:out.sha256)=*value;
        }else if(*key=="size"){
            whitespace();size_t start=cursor;while(cursor<text.size()&&text[cursor]>='0'&&text[cursor]<='9'){
                if(cursor-start>9)return {};out.size=out.size*10+unsigned(text[cursor++]-'0');
            }
            if(cursor==start||(cursor-start>1&&text[start]=='0'))return {};
        }else return {};
        if(!consume(i==2?'}':','))return {};
    }
    whitespace();if(cursor!=text.size()||!version(out.version)||out.sha256.size()!=64||out.size<1024||out.size>512ull*1024*1024)return {};
    for(char c:out.sha256)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return {};
    return out;
}
inline std::wstring quote(const std::wstring& arg){
    std::wstring out=L"\"";size_t slashes=0;
    for(wchar_t c:arg){if(c==L'\\'){++slashes;continue;}out.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;out+=c;}
    out.append(slashes*2,L'\\');out+=L'"';return out;
}
inline std::string extension(const std::filesystem::path& path){
    auto value=path.extension().u8string();std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});return value;
}
inline bool recognized(const std::filesystem::path& path,bool audio){
    static const std::set<std::string> video={".mp4",".mkv",".avi",".mov",".wmv",".webm",".m4v",".mpeg",".mpg",".ts",".mts",".m2ts",".flv",".ogv",".vob",".3gp",".hevc",".h264",".mxf",".divx",".asf",".rm",".rmvb"};
    static const std::set<std::string> music={".mp3",".flac",".wav",".aac",".m4a",".ogg",".opus",".wma",".aiff",".alac"};
    return (audio?music:video).count(extension(path))!=0;
}
inline std::vector<std::filesystem::path> scan(const std::vector<std::filesystem::path>& folders,bool audio){
    std::set<std::filesystem::path> result;
    for(const auto& root:folders){std::error_code ec;
        std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;
        while(it!=end){if(it->is_regular_file(ec)&&recognized(it->path(),audio))result.insert(it->path());it.increment(ec);if(ec)ec.clear();}
    }
    return {result.begin(),result.end()};
}
inline std::wstring clock(int64_t ms){
    auto seconds=std::max<int64_t>(0,ms/1000);auto two=[](int64_t n){auto text=std::to_wstring(n);return text.size()<2?L"0"+text:text;};
    return two(seconds/3600)+L":"+two(seconds/60%60)+L":"+two(seconds%60);
}
}
