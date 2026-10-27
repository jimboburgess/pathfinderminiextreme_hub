#include "service.h"
#include "server_limits.h"
#include "screen_stream.h"
#include "runtime.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
using namespace pfme::server;
namespace {
unsigned checks=0;
json::array lastPackets;
void check(bool value,const char* label) { if(!value)throw std::runtime_error(label);++checks; }
json::object call(Service& service,ConnectionState& c,const char* type,json::object fields={}) {
    fields["v"]=1;fields["id"]="test";fields["type"]=type;
    // Unit tests bypass only the wall-clock window; real transport rate limits
    // are exercised separately by pfme_websocket_security.
    c.window=std::chrono::steady_clock::now()-std::chrono::seconds(2);
    auto reply=service.handle(c,json::serialize(fields));
    check(reply.text.size()<=MAX_RESPONSE_BYTES,"bounded response");
    return json::parse(reply.text).as_object();
}
bool good(const json::object& r) {return r.at("ok").as_bool();}
bool error(const json::object& r,const char* code) {return !good(r) && r.at("error").at("code").as_string()==code;}
size_t number(const json::value& value) {return value.is_uint64()?value.as_uint64():size_t(value.as_int64());}
std::vector<uint8_t> decode(const json::string& text) {
    const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> bytes;uint32_t bits=0;unsigned count=0;
    for(char c:text) {if(c=='=')break;auto v=alphabet.find(c);check(v<64,"base64 alphabet");bits=(bits<<6)|uint32_t(v);count+=6;if(count>=8){count-=8;bytes.push_back(uint8_t(bits>>count));}}
    return bytes;
}
std::vector<uint16_t> frame(Service& service,ConnectionState& owner,uint64_t& revision,std::vector<uint16_t> previous,bool changeWhilePending=false) {
    lastPackets.clear();
    size_t offset=0;std::vector<uint8_t> bytes;
    for(int chunks=0;chunks<32;chunks++) {
        auto reply=call(service,owner,"screen",{{"since",std::to_string(revision)},{"offset",offset}});
        check(good(reply),"owner screen accepted");const auto& data=reply.at("data").as_object();
        lastPackets.push_back(data);
        check(number(data.at("offset"))==offset,"ordered chunk offset");
        auto part=decode(data.at("pixels").as_string());check(part.size()<=9216,"bounded chunk");bytes.insert(bytes.end(),part.begin(),part.end());offset+=part.size();
        if(chunks==0 && changeWhilePending) {
            check(error(call(service,owner,"screen",{{"since",std::to_string(revision)},{"offset",offset+1}}),"InvalidCursor"),"invalid offset rejected");
            check(good(call(service,owner,"input",{{"action","a"},{"down",true}})),"real title A accepted");
            pfme::runtime::advance(30);
            check(good(call(service,owner,"input",{{"action","a"},{"down",false}})),"release accepted");
        }
        if(data.at("more").as_bool())continue;
        check(bytes.size()==number(data.at("total")),"complete immutable frame");
        for(size_t p=0;p<bytes.size();) {
            check(p+4<=bytes.size(),"run header");size_t first=bytes[p]|size_t(bytes[p+1])<<8,encoded=bytes[p+2]|size_t(bytes[p+3])<<8;p+=4;
            const bool repeated=(encoded&0x8000)!=0;const size_t length=encoded&32767,size=repeated?2:length*2;
            check(length && first+length<=57600 && p+size<=bytes.size(),"run bounds");
            for(size_t i=0;i<length;i++){const auto at=p+(repeated?0:i*2);previous[first+i]=uint16_t(bytes[at]|uint16_t(bytes[at+1])<<8);}
            p+=size;
        }
        revision=std::stoull(std::string(data.at("frame").as_string()));
        return previous;
    }
    throw std::runtime_error("frame exceeded chunk bound");
}
}
int main() {
    try {
        const auto secret=randomCredential();SecretDigest digest{};check(credentialDigest(secret,digest),"gate digest");
        Service service(digest,2);ConnectionState owner,other,reconnected;owner.serial=1;other.serial=2;reconnected.serial=3;
        check(error(call(service,owner,"screen",{{"since","0"},{"offset",0}}),"NotAuthenticated"),"screen requires authentication");
        check(error(call(service,owner,"input",{{"action","a"},{"down",true}}),"NotAuthenticated"),"input requires authentication");
        check(good(call(service,owner,"authenticate",{{"secret",secret}})),"owner authenticated");
        check(error(call(service,owner,"play"),"NotBound"),"play requires owned participant");
        auto joined=call(service,owner,"join",{{"name","Tester"}});check(good(joined),"owner joins");
        const std::string proof(joined.at("data").at("reconnect_token").as_string());
        check(good(call(service,owner,"play")),"owner claims production runtime");
        check(good(call(service,other,"authenticate",{{"secret",secret}})),"second connection authenticated");
        check(good(call(service,other,"join",{{"name","Observer"}})),"second participant joins");
        check(error(call(service,other,"play"),"RuntimeOccupied"),"another participant cannot claim view");
        check(error(call(service,other,"screen",{{"since","0"},{"offset",0}}),"OwnershipRequired"),"no cross-player screen leak");
        check(error(call(service,other,"input",{{"action","a"},{"down",true}}),"OwnershipRequired"),"no cross-player control");
        check(error(call(service,owner,"input",{{"action","a"},{"down",true},{"hp",999}}),"InvalidRequest"),"client HP rejected");
        check(error(call(service,owner,"input",{{"action","teleport"},{"down",true}}),"InvalidAction"),"only hardware intents accepted");
        check(error(call(service,owner,"input",{{"action","a"},{"down",1}}),"InvalidRequest"),"strict input type");
        check(error(call(service,owner,"move",{{"direction","east"}}),"RuntimeMode"),"no competing topology movement");
        const std::vector<uint16_t> title(pfme::runtime::pixels(),pfme::runtime::pixels()+57600);
        uint64_t revision=0;auto presented=frame(service,owner,revision,std::vector<uint16_t>(57600),true);
        check(presented==title,"in-flight title does not mix later game state");
        presented=frame(service,owner,revision,std::move(presented));
        check(std::equal(presented.begin(),presented.end(),pfme::runtime::pixels()),"browser pixels match actual production screen");
        check(presented!=title,"title A entered actual character creation");
        for(int selection=0;selection<2;selection++) {
            check(good(call(service,owner,"input",{{"action","encoder"},{"down",true}})),"creation select");
            pfme::runtime::advance(30);
            check(good(call(service,owner,"input",{{"action","encoder"},{"down",false}})),"creation release");
            for(int tick=0;tick<35;tick++)pfme::runtime::advance(10);
        }
        presented=frame(service,owner,revision,std::move(presented));
        check(std::equal(presented.begin(),presented.end(),pfme::runtime::pixels()),"full character sheet matches production pixels");
        check(good(call(service,reconnected,"authenticate",{{"secret",secret}})),"resume authenticated");
        check(error(call(service,reconnected,"reconnect",{{"token",proof}}),"AlreadyConnected"),"no active takeover");
        service.detach(owner);
        check(good(call(service,reconnected,"reconnect",{{"token",proof}})),"proof resumes owned participant");
        check(good(call(service,reconnected,"play")),"same owner resumes view");
        revision=0;auto resumed=frame(service,reconnected,revision,std::vector<uint16_t>(57600));
        check(resumed==presented,"reconnect preserves game state and resynchronizes full pixels");
        std::ofstream("resync-packets.json")<<json::serialize(lastPackets);
        std::ofstream pixelFile("resync-pixels.rgb565",std::ios::binary);
        for(auto pixel:resumed){pixelFile.put(char(pixel&255));pixelFile.put(char(pixel>>8));}
        check(good(call(service,reconnected,"leave")),"leave revokes ownership");
        check(error(call(service,other,"play"),"RuntimeRetired"),"retired global runtime cannot leak to another owner");
        std::cout<<"playable ownership/frame checks="<<checks<<" passed; production input and atomic view verified\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
