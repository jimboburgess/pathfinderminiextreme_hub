#include "service.h"
#include "connection_policy.h"
#include <iostream>
#include <stdexcept>
using namespace pfme::server;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
 using namespace std::chrono;auto now=ConnectionClock::now();const auto began=now;
 Liveness live;live.traffic(now);live.pingAt=now;now+=seconds(15);check(live.pingDue(now),"idle transport needs 15s ping");now+=seconds(29);check(!live.stale(now),"44 seconds is not stale");now+=seconds(1);check(live.stale(now),"45-second dead transport");
 for(unsigned i=0;i<20;++i){now+=seconds(15);live.traffic(now);check(!live.stale(now),"several idle gameplay minutes with pong remain connected");}
 now=began;Service service({},2,true,true,[&]{return now;});ConnectionState connections[22];uint64_t seq[22]{};
 auto call=[&](unsigned i,const char* type,json::object fields=json::object{}){fields["v"]=1;fields["id"]=std::to_string(++seq[i]);fields["type"]=type;connections[i].window=now-seconds(2);auto result=service.handle(connections[i],json::serialize(fields));return json::parse(result.text).as_object();};
 for(unsigned i=0;i<20;++i){connections[i].serial=i+1;connections[i].verifiedPrincipal="acct-life-"+std::to_string(i);check(call(i,"authenticate").at("ok").as_bool(),"20 distinct verified authentications");check(call(i,"join",{{"name","Hero"+std::to_string(i)}}).at("ok").as_bool(),"20 reserved characters");}
 check(call(19,"events",{{"since","0"}}).at("error").as_object().at("code")=="RuntimeMode","legacy events cannot leak another bank");
 check(call(19,"map").at("error").as_object().at("code")=="RuntimeMode","legacy graph is not the live party world");
 connections[20].serial=100;connections[20].verifiedPrincipal="acct-twentyfirst";check(call(20,"authenticate").at("error").as_object().at("code")=="ServerFull","21st distinct account cleanly rejected");
 const auto reserved=connections[0].player;service.detach(connections[0]);now+=seconds(119);service.tick();
 connections[21].serial=200;connections[21].verifiedPrincipal="acct-life-0";check(call(21,"authenticate").at("ok").as_bool(),"119-second reconnect does not consume capacity");check(call(21,"reconnect").at("ok").as_bool()&&connections[21].player==reserved,"same reserved slot");
 // A second authenticated socket replaces the first, never grants two controls.
 connections[0]=ConnectionState{};connections[0].verifiedPrincipal="acct-life-0";connections[0].serial=300;seq[0]=0;check(call(0,"authenticate").at("ok").as_bool(),"new verified socket");check(service.replaced(connections[21]),"old socket is explicitly superseded");check(call(0,"reconnect").at("ok").as_bool()&&connections[0].player==reserved,"duplicate restores original capability");
 service.detach(connections[0]);now+=seconds(120);service.tick();
 check(call(20,"authenticate").at("ok").as_bool(),"expired reservation admits new distinct account");check(call(20,"join",{{"name","Replacement"}}).at("ok").as_bool(),"reclaimed capacity after safe expiry");
 std::cout<<"PASS fake-clock 15/45 keepalive, idle minutes, 20/21 accounts, 119s reconnect, 120s expiry, duplicate replacement and stable account binding\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
