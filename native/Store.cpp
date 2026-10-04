#include "Runtime.hpp"
#include <Unreal/Core/Windows/AllowWindowsPlatformTypes.hpp>
#include <Windows.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <future>

namespace Wardrobe {
namespace {
std::string utf8(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);std::string r(n,0);WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),r.data(),n,nullptr,nullptr);return r;}
std::wstring wide(const std::string& s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);if(!n)throw std::runtime_error("Invalid UTF-8");std::wstring r(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),r.data(),n);return r;}
bool readChoice(std::istream& input,Choice& c,unsigned slot){int mode;std::string row;if(!(input>>mode>>std::quoted(row))||mode<0||mode>2||row.size()>2048)return false;c.mode=static_cast<Choice::Mode>(mode);c.row=wide(row);return !(c.mode==Choice::Mode::Hidden&&!hideAllowed(static_cast<Slot>(slot)))&&(c.mode!=Choice::Mode::Look||!c.row.empty());}
void choice(std::ostream& out,const Choice& c){out<<static_cast<int>(c.mode)<<' '<<std::quoted(utf8(c.row));}
struct SaveJob {std::filesystem::path directory;std::string data;};
std::optional<SaveJob> queuedSave;
std::future<bool> saving;
bool persist(SaveJob job){
    // The worker receives only owned text and paths. It never touches runtime,
    // Unreal objects, Lua, logging or reflection.
    try{
        auto path=job.directory/L"wardrobe.dat",temp=job.directory/L"wardrobe.dat.tmp",backup=job.directory/L"wardrobe.dat.bak";
        HANDLE h=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
        DWORD n{};bool ok=WriteFile(h,job.data.data(),static_cast<DWORD>(job.data.size()),&n,nullptr)&&n==job.data.size()&&FlushFileBuffers(h);CloseHandle(h);if(!ok)return false;
        if(std::filesystem::exists(path))return ReplaceFileW(path.c_str(),temp.c_str(),backup.c_str(),0,nullptr,nullptr)!=0;
        return MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH)!=0;
    }catch(...){return false;}
}
}
bool storePending(){return saving.valid()||queuedSave.has_value();}
void pollStore(){
    if(saving.valid()){
        if(saving.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        if(!saving.get())warn(L"Saving wardrobe.dat failed; previous saved choices remain available. Check folder write access.");
    }
    if(queuedSave){
        auto job=std::move(*queuedSave);queuedSave.reset();
        try{saving=std::async(std::launch::async,persist,std::move(job));}
        catch(...){warn(L"Could not start the wardrobe save worker; previous saved choices remain available.");}
    }
}
void finishStore(){while(storePending()){if(saving.valid())saving.wait();pollStore();}}
void readStore(){
    auto path=runtime.directory/L"wardrobe.dat";if(!std::filesystem::exists(path))return;
    try{
        if(std::filesystem::file_size(path)>4*1024*1024)throw std::runtime_error("Oversized wardrobe file");
        std::ifstream in(path,std::ios::binary);std::string magic;unsigned version;
        if(!(in>>magic>>version)||magic!="WardrobeTransmog"||version!=1)throw std::runtime_error("Unknown wardrobe format");
        Model result;std::string token;
        std::array<std::array<bool,slotCount>,2> seen{};bool seenAll=false;
        while(in>>token){
            if(token=="look"){
                unsigned set,slot;if(!(in>>set>>slot)||set>1||slot>=slotCount||seen[set][slot]||!readChoice(in,result.sets[set][slot],slot))throw std::runtime_error("Invalid appearance");seen[set][slot]=true;
            }else if(token=="outfit"){
                unsigned index;if(!(in>>index)||index>=3||result.presets[index])throw std::runtime_error("Invalid outfit");Outfit o;
                for(unsigned i=0;i<slotCount;++i)if(!readChoice(in,o[i],i))throw std::runtime_error("Invalid outfit appearance");result.presets[index]=o;
            }else if(token=="known"){
                std::string key;if(!(in>>std::quoted(key))||key.size()>2048||result.collected.size()>=16384)throw std::runtime_error("Invalid collection");result.collected.insert(wide(key));
            }else if(token=="all") {unsigned v;if(seenAll||!(in>>v)||v>1)throw std::runtime_error("Invalid filter");result.allLooks=v!=0;seenAll=true;}
            else throw std::runtime_error("Unknown wardrobe field");
        }
        if(!in.eof()||!seenAll||!std::all_of(seen.begin(),seen.end(),[](auto& a){return std::all_of(a.begin(),a.end(),[](bool b){return b;});}))throw std::runtime_error("Incomplete wardrobe file");runtime.model=std::move(result);
    }catch(...){
        // Never overwrite unreadable preferences with empty defaults.
        auto backup=path;backup+=L".unreadable";
        unsigned suffix=0;while(std::filesystem::exists(backup)){backup=path;backup+=L".unreadable."+std::to_wstring(++suffix);}
        try{std::filesystem::copy_file(path,backup);}catch(...){runtime.persistenceBlocked=true;}
        warn(runtime.persistenceBlocked?L"Could not read or back up wardrobe.dat; saving is disabled to preserve it.":L"Could not read wardrobe.dat; a separate .unreadable backup was created.");
    }
}
bool writeStore(){
    if(!runtime.dirty)return true;
    if(runtime.persistenceBlocked){runtime.dirty=false;warn(L"Saving is disabled because the unreadable wardrobe file could not be backed up.");return false;}
    try{
        std::ostringstream out;out<<"WardrobeTransmog 1\nall "<<(runtime.model.allLooks?1:0)<<'\n';
        for(unsigned set=0;set<2;++set)for(unsigned slot=0;slot<slotCount;++slot){out<<"look "<<set<<' '<<slot<<' ';choice(out,runtime.model.sets[set][slot]);out<<'\n';}
        for(unsigned i=0;i<3;++i)if(runtime.model.presets[i]){out<<"outfit "<<i;for(const auto& c:*runtime.model.presets[i]){out<<' ';choice(out,c);}out<<'\n';}
        for(const auto& k:runtime.model.collected)out<<"known "<<std::quoted(utf8(k))<<'\n';
        // A busy worker keeps at most one newer snapshot. Closing the menu and
        // the autosave timer never wait for disk flushing or an older write.
        queuedSave=SaveJob{runtime.directory,out.str()};runtime.dirty=false;pollStore();return true;
    }catch(...){runtime.dirty=false;warn(L"Saving wardrobe.dat failed; previous saved choices remain available. Check folder write access.");return false;}
}
}
