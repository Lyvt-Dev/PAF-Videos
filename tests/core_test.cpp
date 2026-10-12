#include "core.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
int checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
int main(){try{
    check(paf::newer("0.1.1","0.1.0"),"upgrade");
    check(paf::newer("0.1.1-fix","0.1.1"),"fix version");
    check(!paf::newer("0.1.0","0.1.1"),"downgrade");
    for(auto text:{"01.2.3","1.2","1.2.3/bad","65536.0.0","1.2.3.4"})check(!paf::version(text),"invalid version");
    auto hash=std::string(64,'a');
    auto good="{\"version\":\"0.1.1\",\"sha256\":\""+hash+"\",\"size\":2048}";
    check(bool(paf::manifest(good)),"manifest");
    check(bool(paf::manifest("{\"sha256\":\""+hash+"\",\"size\":2048,\"version\":\"0.1.0\"}")),"legacy manifest order");
    for(auto text:{"{}","{\"size\":true}","{\"version\":\"0.1.1\",\"version\":\"0.1.1\",\"size\":2048}"})check(!paf::manifest(text),"invalid manifest");
    check(!paf::manifest(good+"junk"),"trailing data");
    check(!paf::manifest(std::string(8193,' ')+good),"manifest size limit");
    check(paf::quote(L"a b")==L"\"a b\"","quote spaces");
    check(paf::quote(L"x\\")==L"\"x\\\\\"","quote trailing slash");
    check(paf::quote(L"a\"b")==L"\"a\\\"b\"","quote embedded quote");
    check(paf::clock(3661000)==L"01:01:01","duration");
    check(paf::recognized("CLIP.MKV",false),"video case");
    check(!paf::recognized("clip.mkv",true),"media distinction");
    auto root=std::filesystem::temp_directory_path()/"paf-native-core-test";
    std::filesystem::create_directories(root/"nested");std::ofstream(root/"nested"/"clip.MKV").put('x');std::ofstream(root/"track.flac").put('x');std::ofstream(root/"notes.txt").put('x');
    check(paf::scan({root,root},false).size()==1,"recursive video deduplication");
    check(paf::scan({root},true).size()==1,"audio scan");std::filesystem::remove_all(root);
    std::cout<<checks<<" checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
