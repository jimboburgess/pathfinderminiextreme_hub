#include "serve_identity.h"
#include "service.h"
#include <iostream>
#include <array>
#include <memory>
#include <stdexcept>
using namespace pfme::server;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try {
    ServeSnapshot policy;
    check(parseServePolicy(R"({"version":1,"revision":1,"users":[{"login":"owner@example.com","principal":"owner"},{"login":"friend@example.com","principal":"friend"}]})",policy),"valid policy");
    auto now=ServeClock::now();ServeLease lease{"owner@example.com","owner",1,now+std::chrono::seconds(900)};
    check(serveLeaseValid(policy,lease,now),"valid lease");
    check(!serveLeaseValid(policy,lease,now+std::chrono::seconds(900)),"absolute expiration at exact deadline");
    check(!serveLeaseValid(policy,lease,now+std::chrono::seconds(901)),"expired lease cannot replay");
    ServeLease friendLease{"friend@example.com","friend",1,now+std::chrono::seconds(900)};
    ServeSnapshot removed;
    check(parseServePolicy(R"({"version":1,"revision":2,"users":[{"login":"owner@example.com","principal":"owner"}]})",removed),"remove only friend");
    retainUnchangedServeAuthorizations(policy,removed);
    check(serveLeaseValid(removed,lease,now),"unaffected owner keeps existing lease");
    check(!serveLeaseValid(removed,friendLease,now),"removed friend denied");
    ServeSnapshot restored;
    check(parseServePolicy(R"({"version":1,"revision":3,"users":[{"login":"owner@example.com","principal":"owner"},{"login":"friend@example.com","principal":"friend"}]})",restored),"explicit friend regrant");
    retainUnchangedServeAuthorizations(removed,restored);
    check(serveLeaseValid(restored,lease,now),"adding friend preserves owner lease");
    check(!serveLeaseValid(restored,friendLease,now),"regrant never resurrects revoked lease");
    friendLease.revision=3;check(serveLeaseValid(restored,friendLease,now),"new friend authorization works");
    ServeSnapshot remapped;
    check(parseServePolicy(R"({"version":1,"revision":4,"users":[{"login":"owner@example.com","principal":"owner"},{"login":"friend@example.com","principal":"different"}]})",remapped),"administrative remapping");
    retainUnchangedServeAuthorizations(restored,remapped);
    check(serveLeaseValid(remapped,lease,now)&&!serveLeaseValid(remapped,friendLease,now),"remapping revokes only changed identity");
    check(!serveLeaseValid(remapped,lease,now+std::chrono::seconds(900)),"updates never extend absolute expiry");
    for(const auto* invalid:{R"({"version":1,"revision":0,"users":[]})",R"({"version":1,"revision":1,"users":[{"login":"a","principal":"same"},{"login":"b","principal":"same"}]})",R"({"version":1,"revision":1,"users":[{"login":"a","principal":"one"},{"login":"a","principal":"two"}]})",R"({"version":1,"revision":1,"users":[],"fallback":true})"})check(!parseServePolicy(invalid,policy),"invalid policy rejected");
    check(validServeOrigin("https://game.test.ts.net"),"private HTTPS origin");
    for(auto origin:{"http://game.test.ts.net","https://game.test.ts.net/","https://game.test.ts.net:443","https://game.test.ts.net@evil.invalid","https://*.ts.net"})check(!validServeOrigin(origin),"ambiguous origin denied");
    SecretDigest gate{};Service service(gate,12345,false,true);
    auto make=[](uint64_t serial,const char* principal){ConnectionState c;c.serial=serial;c.verifiedPrincipal=principal;return c;};
    auto a=make(1,"owner"),b=make(2,"friend"),unknown=make(3,"");
    auto call=[&](ConnectionState& c,unsigned sequence,const char* type,json::object fields=json::object{}){fields["v"]=1;fields["id"]=std::to_string(sequence);fields["type"]=type;return json::parse(service.handle(c,json::serialize(fields)).text);};
    auto ok=[](const json::value& value){return value.at("ok").as_bool();};
    auto code=[](const json::value& value){return std::string(value.at("error").at("code").as_string());};
    check(code(call(unknown,1,"authenticate"))=="IdentityRequired","unverified identity cannot authenticate");
    check(code(call(a,1,"authenticate",{{"secret",std::string(64,'a')}}))=="AuthenticationFailed","no shared gate fallback");
    check(ok(call(a,2,"authenticate"))&&ok(call(b,1,"authenticate")),"independent verified auth");
    auto joined=call(a,3,"join",{{"name","Owner"}});check(ok(joined)&&!joined.at("data").as_object().contains("reconnect_token"),"no browser bearer proof");
    check(code(call(b,2,"reconnect",{{"player_id",0}}))=="OwnershipRequired","numeric takeover rejected");
    check(code(call(b,3,"reconnect"))=="NoCharacter","other principal cannot recover owner");
    service.detach(a);auto resumed=make(4,"owner");check(ok(call(resumed,1,"authenticate")),"fresh verified auth");
    check(code(call(resumed,2,"join",{{"name","Duplicate"}}))=="ResumeRequired","no duplicate occupied character");
    check(ok(call(resumed,3,"reconnect")),"same principal reconnect");
    auto replay=service.handle(resumed,R"({"v":1,"id":"3","type":"leave"})");check(replay.close&&code(json::parse(replay.text))=="ReplayRejected","replay closes before action");
    service.detach(resumed);auto finalOwner=make(5,"owner");
    check(ok(call(finalOwner,1,"authenticate"))&&ok(call(finalOwner,2,"reconnect")),"replay did not alter retained character");
    check(ok(call(finalOwner,3,"leave")),"explicit verified leave");check(code(call(finalOwner,4,"reconnect"))=="NoCharacter","leave revokes ownership");
    // Keep the second full-session fixture off the small Windows thread stack.
    auto groupService=std::make_unique<Service>(gate,54321,false,true);
    std::array<ConnectionState,6> groupClients{};std::array<unsigned,6> sequences{};
    auto groupCall=[&](unsigned index,const char* type,json::object fields=json::object{}){
        fields["v"]=1;fields["id"]=std::to_string(++sequences[index]);fields["type"]=type;
        return json::parse(groupService->handle(groupClients[index],json::serialize(fields)).text);
    };
    for(unsigned i=0;i<6;++i){groupClients[i].serial=100+i;groupClients[i].verifiedPrincipal="fixture-group-"+std::to_string(i);check(ok(groupCall(i,"authenticate")),"six independently authenticated identities");}
    for(unsigned i=0;i<4;++i)check(ok(groupCall(i,"join",{{"name","Fixture"}})),"four occupied characters");
    const auto fullState=groupCall(0,"players").at("data");
    check(code(groupCall(4,"join",{{"name","Waiting"}}))=="Full","fifth occupied slot rejected");
    check(code(groupCall(5,"join",{{"name","Waiting"}}))=="Full","sixth authorized identity waits");
    check(groupCall(0,"players").at("data")==fullState,"full rejection preserves party");
    check(ok(groupCall(3,"disconnect")),"disconnect preserves occupied character");
    check(code(groupCall(4,"join",{{"name","Waiting"}}))=="Full","disconnect never frees ownership");
    check(ok(groupCall(3,"reconnect")),"original owner reconnects without duplicate");
    check(ok(groupCall(3,"leave")),"explicit verified Leave releases slot");
    check(ok(groupCall(4,"join",{{"name","Waiting"}})),"waiting identity joins released slot");
    check(code(groupCall(3,"reconnect"))=="NoCharacter","retired owner cannot take replacement");
    check(code(groupCall(5,"join",{{"name","Waiting"}}))=="Full","party maximum remains four");
    std::cout<<"PASS six authenticated identities, four slots, waiting join after Leave; Serve policy, fixed-clock expiry, individual revocation, unchanged-peer continuity, nonresurrecting regrant, unverified denial, principal binding, reconnect, no bearer leak and replay rejection\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
