#pragma once
// Native account storage only. ESP32 Preferences and save-format bytes stay intact.
#include <array>
#include "controller_capacity.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#ifdef __linux__
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#endif
class CharacterFiles {
    struct Owner {std::string name;int slot=-1;};
    static std::array<Owner,pfme::runtime::NATIVE_CONTROLLER_CAPACITY>& owners(){static std::array<Owner,pfme::runtime::NATIVE_CONTROLLER_CAPACITY> v;return v;}
    static const char* filename(const char* key){
        if(!key)return nullptr;
        if(!std::strcmp(key,"player0"))return "save_slot_1.dat";
        if(!std::strcmp(key,"player1"))return "save_slot_2.dat";
        if(!std::strcmp(key,"player2"))return "save_slot_3.dat";
        if(!std::strcmp(key,"player"))return "save.dat";
        return nullptr;
    }
    static bool valid(const std::string& owner){
        if(owner.size()<10||owner.size()>136||owner.compare(0,8,"account-")!=0||(owner.size()-8)%2)return false;
        for(size_t i=8;i<owner.size();++i)if(!((owner[i]>='0'&&owner[i]<='9')||(owner[i]>='a'&&owner[i]<='f')))return false;
        return true;
    }
#ifdef __linux__
    struct Fd {int value=-1;explicit Fd(int v):value(v){}~Fd(){if(value>=0)::close(value);}operator int()const{return value;}};
    static int directory(const std::string& owner,bool create){
        const char* root=std::getenv("PFME_SAVE_DIR");if(!root||root[0]!='/'||!valid(owner))return -1;
        Fd parent(::open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));struct stat s{};
        if(parent<0||::fstat(parent,&s)||s.st_uid!=::geteuid()||(s.st_mode&0777)!=0700)return -1;
        if(create){
            if(::mkdirat(parent,owner.c_str(),0700)==0){if(::fsync(parent))return -1;}
            else if(errno!=EEXIST)return -1;
        }
        Fd child(::openat(parent,owner.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
        if(child<0||::fstat(child,&s)||s.st_uid!=::geteuid()||(s.st_mode&0777)!=0700)return -1;
        return ::fcntl(child,F_DUPFD_CLOEXEC,0);
    }
    static int record(int dir,const char* name){
        Fd f(::openat(dir,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));struct stat s{};
        if(f<0||::fstat(f,&s)||!S_ISREG(s.st_mode)||s.st_nlink!=1||s.st_uid!=::geteuid()||(s.st_mode&0777)!=0600||s.st_size<1||s.st_size>65536)return -1;
        return ::fcntl(f,F_DUPFD_CLOEXEC,0);
    }
#else
    static std::filesystem::path path(const std::string& owner,const char* key,bool create){
        const char* root=std::getenv("PFME_SAVE_DIR");if(!root||!*root||!valid(owner)||!filename(key))return {};
        std::filesystem::path base(root);std::error_code e;
        if(!base.is_absolute()||!std::filesystem::is_directory(base,e)||std::filesystem::is_symlink(std::filesystem::symlink_status(base,e)))return {};
        auto account=base/owner;
        if(create)std::filesystem::create_directory(account,e);
        if(!std::filesystem::is_directory(account,e)||std::filesystem::is_symlink(std::filesystem::symlink_status(account,e)))return {};
#ifdef _WIN32
        const auto attr=GetFileAttributesW(account.c_str());
        if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT))return {};
#endif
        auto file=account/filename(key);
        if(std::filesystem::is_symlink(std::filesystem::symlink_status(file,e)))return {};
        return file;
    }
#endif
public:
    static bool required(){const char* s=std::getenv("PFME_REQUIRE_ACCOUNT_SAVES");return s&&std::strcmp(s,"1")==0;}
    static bool bind(unsigned id,const std::string& principal){
        if(id>=pfme::runtime::NATIVE_CONTROLLER_CAPACITY||principal.empty()||principal.size()>64)return false;
        static const char hex[]="0123456789abcdef";std::string name="account-";
        for(unsigned char c:principal){if(c<33||c>126)return false;name+=hex[c>>4];name+=hex[c&15];}
        for(unsigned other=0;other<pfme::runtime::NATIVE_CONTROLLER_CAPACITY;++other)if(other!=id&&owners()[other].name==name)return false;
        auto& o=owners()[id];if(!o.name.empty()&&o.name!=name)return false;o.name=name;return true;
    }
    static std::string owner(unsigned id){return id<pfme::runtime::NATIVE_CONTROLLER_CAPACITY?owners()[id].name:std::string();}
    static int active(unsigned id){return id<pfme::runtime::NATIVE_CONTROLLER_CAPACITY?owners()[id].slot:-1;}
    static void activate(unsigned id,unsigned slot){if(id<pfme::runtime::NATIVE_CONTROLLER_CAPACITY&&slot<3)owners()[id].slot=int(slot);}
    static void deactivate(unsigned id){if(id<pfme::runtime::NATIVE_CONTROLLER_CAPACITY)owners()[id].slot=-1;}
    static void retire(unsigned id){if(id<pfme::runtime::NATIVE_CONTROLLER_CAPACITY)owners()[id]=Owner{};}
    static void reset(){for(auto& o:owners())o=Owner{};}
    static size_t length(const std::string& owner,const char* key) noexcept {try{
        const char* name=filename(key);if(!name)return 0;
#ifdef __linux__
        Fd dir(directory(owner,false));if(dir<0)return 0;Fd f(record(dir,name));struct stat s{};
        return f>=0&&::fstat(f,&s)==0?size_t(s.st_size):0;
#else
        auto p=path(owner,key,false);if(p.empty())return 0;std::error_code e;auto n=std::filesystem::file_size(p,e);return !e&&n<=65536?size_t(n):0;
#endif
    }catch(...){return 0;}}
    static size_t read(const std::string& owner,const char* key,void* data,size_t capacity) noexcept {try{
        const char* name=filename(key);if(!name||!data)return 0;
#ifdef __linux__
        Fd dir(directory(owner,false));if(dir<0)return 0;Fd f(record(dir,name));struct stat s{};
        if(f<0||::fstat(f,&s)||size_t(s.st_size)>capacity)return 0;
        size_t at=0;while(at<size_t(s.st_size)){auto n=::read(f,static_cast<char*>(data)+at,size_t(s.st_size)-at);if(n<0&&errno==EINTR)continue;if(n<=0)return 0;at+=size_t(n);}return at;
#else
        auto p=path(owner,key,false);const auto n=length(owner,key);if(p.empty()||!n||n>capacity)return 0;std::ifstream in(p,std::ios::binary);in.read(static_cast<char*>(data),n);return in&&size_t(in.gcount())==n?n:0;
#endif
    }catch(...){return 0;}}
    static size_t write(const std::string& owner,const char* key,const void* data,size_t size) noexcept {try{
        const char* name=filename(key);if(!name||!data||!size||size>65536)return 0;
        std::vector<unsigned char> previous(size);if(read(owner,key,previous.data(),size)==size&&!std::memcmp(previous.data(),data,size))return size;
#ifdef __linux__
        Fd dir(directory(owner,true));if(dir<0)return 0;
        struct stat destination{};
        if(::fstatat(dir,name,&destination,AT_SYMLINK_NOFOLLOW)==0){Fd existing(record(dir,name));if(existing<0)return 0;}
        else if(errno!=ENOENT)return 0;
        static uint64_t sequence=0;
        const std::string temp=std::string(name)+".tmp."+std::to_string(::getpid())+"."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"."+std::to_string(++sequence);
        Fd f(::openat(dir,temp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));if(f<0)return 0;
        bool good=true;size_t at=0;while(at<size){auto n=::write(f,static_cast<const char*>(data)+at,size-at);if(n<0&&errno==EINTR)continue;if(n<=0){good=false;break;}at+=size_t(n);}
        if(good)good=::fsync(f)==0;
        if(good)good=::renameat(dir,temp.c_str(),dir,name)==0;
        if(good)good=::fsync(dir)==0;
        if(!good)::unlinkat(dir,temp.c_str(),0);
        return good?size:0;
#else
        auto destination=path(owner,key,true);if(destination.empty())return 0;auto temp=destination;temp+=".tmp";
        std::error_code e;if(std::filesystem::is_symlink(std::filesystem::symlink_status(temp,e)))return 0;
        {std::ofstream out(temp,std::ios::binary|std::ios::trunc);out.write(static_cast<const char*>(data),size);out.flush();if(!out)return 0;}
#ifdef _WIN32
        bool good=MoveFileExW(temp.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        bool good=std::rename(temp.c_str(),destination.c_str())==0;
#endif
        if(!good)std::filesystem::remove(temp,e);return good?size:0;
#endif
    }catch(...){return 0;}}
    static bool remove(const std::string& owner,const char* key) noexcept {try{
        const char* name=filename(key);if(!name)return false;
#ifdef __linux__
        Fd dir(directory(owner,false));if(dir<0)return false;Fd f(record(dir,name));return f>=0&&::unlinkat(dir,name,0)==0&&::fsync(dir)==0;
#else
        auto p=path(owner,key,false);std::error_code e;return !p.empty()&&std::filesystem::remove(p,e)&&!e;
#endif
    }catch(...){return false;}}
};
