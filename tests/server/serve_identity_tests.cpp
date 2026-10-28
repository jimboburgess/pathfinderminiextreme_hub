#include "serve_identity.h"
#include "service.h"
#include <iostream>
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
    ++policy.revision;check(!serveLeaseValid(policy,lease,now),"revision change revokes lease");
    --policy.revision;policy.users.erase(lease.login);check(!serveLeaseValid(policy,lease,now),"removed identity denied");
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
    std::cout<<"PASS Serve policy, fixed-clock expiry, revision revocation, unverified denial, principal binding, reconnect, no bearer leak and replay rejection\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
