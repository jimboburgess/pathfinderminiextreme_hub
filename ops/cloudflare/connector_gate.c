/* Connector pre-exec interlock. No network calls; root releases a fresh nonce
 * only AFTER verifying this container's private network-namespace filter.
 * Never added to the game/gateway; cloudflared itself remains unprivileged.
 */
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
int main(int argc,char **argv) {
    if(argc<4||geteuid()==0||strlen(argv[2])!=64)return 2;
    for(const char *p=argv[2];*p;++p)if(!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')))return 2;
    struct timespec start,now,pause={0,100000000};
    if(clock_gettime(CLOCK_MONOTONIC,&start))return 1;
    for(;;) {
        int fd=open(argv[1],O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(fd>=0) {
            struct stat s;char nonce[65];ssize_t n=-1;
            if(!fstat(fd,&s)&&S_ISREG(s.st_mode)&&s.st_uid==0&&!(s.st_mode&0022)&&s.st_nlink==1&&s.st_size==64)
                n=read(fd,nonce,sizeof nonce);
            close(fd);
            if(n==64&&!memcmp(nonce,argv[2],64)) {execv(argv[3],argv+3);return 1;}
            return 1;
        }
        if(clock_gettime(CLOCK_MONOTONIC,&now)||now.tv_sec-start.tv_sec>=120)return 1;
        nanosleep(&pause,0);
    }
}
