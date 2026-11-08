#pragma once
#ifdef __linux__
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <ctime>
#include <stdexcept>
#include <string>
namespace pfme::server {
// Local event-loop heartbeat, never an HTTP or authentication bypass endpoint.
inline bool runtimeHealthy(const char* path) {
    const int fd=::open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return false;
    struct stat s{};const bool valid=::fstat(fd,&s)==0&&S_ISREG(s.st_mode)&&
        s.st_uid==::geteuid()&&(s.st_mode&0777)==0600&&s.st_nlink==1;
    ::close(fd);const auto now=::time(nullptr);
    return valid&&now>=s.st_mtime&&now-s.st_mtime<20;
}
class RuntimeHealth {
    std::string path_;int fd_=-1;
public:
    explicit RuntimeHealth(std::string path):path_(std::move(path)) {
        fd_=::open(path_.c_str(),O_WRONLY|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);
        struct stat s{};
        if(fd_<0||::fstat(fd_,&s)!=0||!S_ISREG(s.st_mode)||s.st_uid!=::geteuid()||
           (s.st_mode&0777)!=0600||s.st_nlink!=1||::flock(fd_,LOCK_EX|LOCK_NB)!=0) {
            if(fd_>=0)::close(fd_);
            throw std::runtime_error("unsafe runtime health path");
        }
        pulse();
    }
    void pulse(){if(::futimens(fd_,nullptr)!=0)throw std::runtime_error("runtime health update failed");}
    ~RuntimeHealth(){::close(fd_);}
};
}
#endif
