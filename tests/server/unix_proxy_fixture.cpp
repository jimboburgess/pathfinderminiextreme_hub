// Test-only trusted proxy fixture. Never installed or used as an ingress service.
// stdin/stdout JSON protocol; no TCP listener and only the two explicitly mounted paths.
#include <boost/json/src.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <array>
#include <cstring>
#include <iostream>
#include <fstream>
#include <netinet/in.h>
#include <cerrno>
#include <sstream>
#include <stdexcept>
namespace j=boost::json;
std::array<int,8> clients{{-1,-1,-1,-1,-1,-1,-1,-1}};
void sendAll(int fd,const std::string& text){std::size_t at=0;while(at<text.size()){const auto n=::send(fd,text.data()+at,text.size()-at,MSG_NOSIGNAL);if(n<=0)throw std::runtime_error("send");at+=n;}}
std::string exact(int fd,std::size_t count){std::string text(count,'\0');std::size_t at=0;while(at<count){const auto n=::recv(fd,text.data()+at,count-at,0);if(n<=0)throw std::runtime_error("closed");at+=n;}return text;}
void frame(int fd,std::string payload,unsigned char opcode=1){std::string out;out+=char(0x80|opcode);const auto n=payload.size();if(n<126)out+=char(0x80|n);else{out+=char(0xfe);out+=char(n>>8);out+=char(n);}const char mask[]={1,2,3,4};out.append(mask,4);for(std::size_t i=0;i<n;++i)out+=char(payload[i]^mask[i%4]);sendAll(fd,out);}
j::value receive(int fd){std::string message;for(;;){auto h=exact(fd,2);unsigned op=h[0]&15;std::size_t n=static_cast<unsigned char>(h[1])&127;if(n==126){auto a=exact(fd,2);n=(static_cast<unsigned char>(a[0])<<8)|static_cast<unsigned char>(a[1]);}if(n>16384||n==127)throw std::runtime_error("frame bound");auto data=exact(fd,n);if(op==9){frame(fd,data,10);continue;}if(op==8)throw std::runtime_error("closed");if(op!=1&&op!=0)throw std::runtime_error("opcode");message+=data;if(message.size()>16384)throw std::runtime_error("message bound");if(h[0]&0x80)return j::parse(message);}}

int connectLocal(const char* path){int fd=::socket(AF_UNIX,SOCK_STREAM,0);if(fd<0)throw std::runtime_error("socket");sockaddr_un address{};address.sun_family=AF_UNIX;if(std::strlen(path)>=sizeof(address.sun_path))throw std::runtime_error("path");std::strcpy(address.sun_path,path);timeval timeout{4,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));if(::connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0){::close(fd);throw std::runtime_error("connect");}return fd;}
std::string str(const j::object& o,const char* key,const std::string& fallback=""){auto p=o.if_contains(key);return p?std::string(p->as_string()):fallback;}
int main(int argc,char** argv){if(argc!=4)return 2;const std::string host=argv[3];std::string line;
while(std::getline(std::cin,line)){try{const auto q=j::parse(line).as_object();const auto op=str(q,"op");j::object reply;
 if(op=="info")reply={{"uid",::getuid()},{"gid",::getgid()},{"pid",::getpid()}};
 else if(op=="permissions"){
  auto metadata=[](const std::string& path){struct stat s{};if(::lstat(path.c_str(),&s)!=0)throw std::runtime_error("stat");return j::object{{"uid",s.st_uid},{"gid",s.st_gid},{"mode",s.st_mode&07777},{"socket",bool(S_ISSOCK(s.st_mode))},{"regular",bool(S_ISREG(s.st_mode))},{"directory",bool(S_ISDIR(s.st_mode))}};};
  const std::string socket=argv[1],policy=argv[2];
  const auto directory=socket.substr(0,socket.find_last_of('/'));
  const auto policyDirectory=policy.substr(0,policy.find_last_of('/'));
  reply={{"socket",metadata(socket)},{"directory",metadata(directory)},{"policy",metadata(policy)},{"policy_directory",metadata(policyDirectory)},{"parent",metadata(directory.substr(0,directory.find_last_of('/')))}};
 }
 else if(op=="isolation"){
  std::ifstream status("/proc/self/status");std::string text;bool nnp=false,caps=false;
  while(std::getline(status,text)){if(text.find("NoNewPrivs:")==0)nnp=text.find('1')!=std::string::npos;if(text.find("CapEff:")==0){std::istringstream parser(text.substr(7));unsigned long long value=1;parser>>std::hex>>value;caps=bool(parser)&&value==0;}}
  int test=::open("/app/write-probe",O_WRONLY|O_CREAT,0600);const bool readonly=test<0;if(test>=0){::close(test);::unlink("/app/write-probe");}
  int policy=::open(argv[2],O_WRONLY);const bool policyReadOnly=policy<0;if(policy>=0)::close(policy);
  int network=::socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK,0);sockaddr_in target{};target.sin_family=AF_INET;target.sin_port=htons(443);target.sin_addr.s_addr=htonl(0xc0000201);
  const int result=::connect(network,reinterpret_cast<sockaddr*>(&target),sizeof(target));const int error=errno;::close(network);
  reply={{"uid",::getuid()},{"no_new_privileges",nnp},{"zero_capabilities",caps},{"image_not_writable",readonly},{"policy_not_writable",policyReadOnly},
    {"no_docker_socket",::access("/var/run/docker.sock",F_OK)!=0},{"no_home_secrets",::access("/home/jimbo/.ssh",F_OK)!=0},{"no_external_route",result<0&&error==ENETUNREACH}};
 }
 else if(op=="policy"){
  const std::string path=argv[2],temporary=path+".new";const auto text=j::serialize(q.at("value"));
  int fd=::open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0644);if(fd<0)throw std::runtime_error("policy create");
  if(q.contains("unsafe_mode")&&q.at("unsafe_mode").as_bool())if(::fchmod(fd,0666)!=0)throw std::runtime_error("fixture mode");
  std::size_t used=0;while(used<text.size()){auto n=::write(fd,text.data()+used,text.size()-used);if(n<=0){::close(fd);throw std::runtime_error("policy write");}used+=n;}
  if(::fsync(fd)!=0){::close(fd);throw std::runtime_error("policy sync");}::close(fd);
  if(::rename(temporary.c_str(),path.c_str())!=0)throw std::runtime_error("policy rename");reply={{"ok",true}};
 }
 else if(op=="open"||op=="get"){
  const int slot=q.at("slot").to_number<int>();if(slot<0||slot>=8)throw std::runtime_error("slot");
  if(clients[slot]>=0){::close(clients[slot]);clients[slot]=-1;}
  int fd=connectLocal(argv[1]);clients[slot]=fd;const auto target=str(q,"path",op=="get"?"/play":"/");
  std::string request="GET "+target+" HTTP/1.1\r\nHost: "+str(q,"host","localhost")+"\r\n";
  if(q.contains("duplicate_host"))request+="Host: "+str(q,"duplicate_host")+"\r\n";
  if(q.contains("login"))request+="Tailscale-User-Login: "+str(q,"login")+"\r\n";
  if(q.contains("duplicate_login"))request+="Tailscale-User-Login: "+str(q,"duplicate_login")+"\r\n";
  if(op=="open")request+="Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n";
  if((op=="open"&&!q.contains("omit_origin"))||q.contains("origin"))request+="Origin: "+str(q,"origin","https://"+host)+"\r\n";
  if(q.contains("duplicate_origin"))request+="Origin: "+str(q,"duplicate_origin")+"\r\n";
  request+="\r\n";sendAll(fd,request);std::string header;
  try{while(header.size()<8192 && (header.size()<4||header.substr(header.size()-4)!="\r\n\r\n"))header+=exact(fd,1);
   if(op=="open"){if(header.find(" 101 ")==std::string::npos)throw std::runtime_error("upgrade");reply={{"ok",true},{"hello",receive(fd)}};}
   else{std::string body;char bytes[4096];ssize_t n;while((n=::recv(fd,bytes,sizeof(bytes),0))>0){body.append(bytes,n);if(body.size()>65536)throw std::runtime_error("body");}reply={{"ok",header.find(" 200 ")!=std::string::npos},{"headers",header},{"body",body}};::close(fd);clients[slot]=-1;}
  }catch(...){::close(fd);clients[slot]=-1;reply={{"ok",false}};}
 }
 else if(op=="rpc"){
  const int slot=q.at("slot").to_number<int>();if(slot<0||slot>=8||clients[slot]<0)throw std::runtime_error("slot");
  try{frame(clients[slot],j::serialize(q.at("request")));reply={{"response",receive(clients[slot])}};}catch(...){reply={{"closed",true}};}
 }
 else if(op=="close"){const int slot=q.at("slot").to_number<int>();if(slot<0||slot>=8)throw std::runtime_error("slot");if(clients[slot]>=0)::close(clients[slot]);clients[slot]=-1;reply={{"ok",true}};}
 else throw std::runtime_error("operation");
 std::cout<<j::serialize(reply)<<std::endl;
}catch(...){std::cout<<"{\"fixture_error\":true}"<<std::endl;}}
for(int fd:clients)if(fd>=0)::close(fd);
}
