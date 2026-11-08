#include "service.h"
#include "runtime.h"
#include <stdexcept>
#include <iostream>
#include <vector>
using namespace pfme::server;
void check(bool v){if(!v)throw std::runtime_error("interact invariant failed");}
json::object call(Service& s,ConnectionState& c,const char* type,json::object f={}){
 f["v"]=1;f["id"]="1";f["type"]=type;c.window=std::chrono::steady_clock::now()-std::chrono::seconds(2);
 return json::parse(s.handle(c,json::serialize(f)).text).as_object();
}
bool ok(const json::object& r){return r.at("ok").as_bool();}
bool error(const json::object& r,const char* e){return !ok(r)&&r.at("error").at("code").as_string()==e;}
int main(){try{
 auto secret=randomCredential();SecretDigest d{};check(credentialDigest(secret,d));Service s(d,2,true);ConnectionState a,b,c;a.serial=1;b.serial=2;c.serial=3;
 check(error(call(s,a,"interact",{{"since","0"},{"offset",0}}),"NotAuthenticated"));
 check(ok(call(s,a,"authenticate",{{"secret",secret}})));auto joined=call(s,a,"join",{{"name","First"}});check(ok(joined));auto proof=joined.at("data").at("reconnect_token");
 check(ok(call(s,a,"play")));
 const auto id=a.player;auto copy=[&](){const auto* p=pfme::runtime::controllerPixels(id);return std::vector<uint16_t>(p,p+57600);};
 const auto title=copy();
 check(error(call(s,a,"interact",{{"since","0"},{"offset",1},{"action","a"},{"down",true}}),"InvalidCursor"));check(copy()==title);
 check(error(call(s,a,"interact",{{"since","0"},{"offset",0},{"action","a"},{"down",1}}),"InvalidRequest"));check(copy()==title);
 check(error(call(s,a,"interact",{{"since","0"},{"offset",0},{"action","a"},{"down",true},{"player_id",1}}),"InvalidRequest"));check(copy()==title);
 check(ok(call(s,b,"authenticate",{{"secret",secret}})));check(error(call(s,b,"interact",{{"since","0"},{"offset",0}}),"NotBound"));
 auto packet=call(s,a,"interact",{{"since","0"},{"offset",0},{"action","a"},{"down",true}});check(ok(packet));check(packet.if_contains("audio"));
 auto base=packet.at("data").at("base");uint64_t offset=0;
 while(packet.at("data").at("more").as_bool()){
  const auto& pixels=packet.at("data").at("pixels").as_string();offset+=pixels.size()/4*3-(pixels.back()=='=')-(pixels.size()>1&&pixels[pixels.size()-2]=='=');
  packet=call(s,a,"interact",{{"since",base},{"offset",offset}});check(ok(packet));
 }
 const auto since=packet.at("data").at("frame");
 check(error(call(s,a,"interact",{{"since","0"},{"offset",0}}),"InvalidCursor"));
 check(ok(call(s,a,"interact",{{"since",since},{"offset",0},{"action","a"},{"down",false}})));
 s.detach(a);check(ok(call(s,c,"authenticate",{{"secret",secret}})));check(ok(call(s,c,"reconnect",{{"token",proof}})));check(ok(call(s,c,"play")));
 check(error(call(s,a,"interact",{{"since","0"},{"offset",0}}),"NotBound"));
 check(ok(call(s,c,"interact",{{"since","0"},{"offset",0}})));
 std::cout<<"Combined input/frame: authentication, ownership, strict schema, mutation-free invalid cursor, audio, immutable chunks and reconnect passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
