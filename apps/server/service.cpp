#include "service.h"
#include "server_limits.h"
#include <stdexcept>
#include <charconv>
#include <iostream>
#ifdef PFME_HAS_PLAYABLE
#include "runtime.h"
#include "character_saves.h"
#endif

namespace pfme::server
{
namespace
{
json::object playerJson(const SessionPlayer& player)
{ return {{"player_id", player.id}, {"name", player.name}, {"room", player.room}, {"connected", player.connected}}; }
json::value roomJson(uint8_t room)
{ return room == NO_ROOM ? json::value(nullptr) : json::value(room); }
}
Service::Service(const SecretDigest& gate, uint32_t seed, bool partyMode,bool serveMode,Now now) : now_(std::move(now)),game_(partyMode),gate_(gate),serveMode_(serveMode)
{
#ifdef PFME_HAS_PLAYABLE
    seed_=seed;partyMode_=partyMode;lastTick_=now_();
#endif
    if (game_.initialize(seed) != SessionError::Ok) throw std::runtime_error("session initialization failed");
#ifdef PFME_HAS_PLAYABLE
    if(partyMode_)party_=std::make_unique<runtime::PartyRuntime>(game_,seed);
#else
    (void)partyMode;
#endif
}
std::string Service::hello(bool serveMode)
{ return json::serialize(response("", "hello", {{"authentication_required", true}, {"authentication",serveMode?"serve":"development"},{"navigation", "topology-only"}})); }
void Service::tick()
{
    expireReservations();
#ifdef PFME_HAS_PLAYABLE
    const auto now=now_();
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(now-lastTick_).count();
    lastTick_=now;
    if(partyMode_) {
        for(PlayerID id=0;id<runtime::NATIVE_CONTROLLER_CAPACITY;++id)if(partyStarted_[id] && participants_[id].activeConnection) {
            if(now-partyLastInput_[id]>std::chrono::milliseconds(1500))
                party_->input(participants_[id].handle,participants_[id].activeConnection,"release",false);
        }
        bool allocated=false;for(bool started:partyStarted_)allocated=allocated||started;
        if(allocated)party_->tick(static_cast<uint32_t>(std::min<int64_t>(elapsed,100)));
        return;
    }
    if(runtime::isValidController(playOwner_) && !playRetired_ && participants_[playOwner_].activeConnection) {
        if(now-lastInput_>std::chrono::milliseconds(1500)) runtime::releaseInputs();
        runtime::advance(static_cast<uint32_t>(std::min<int64_t>(elapsed,100)));
    }
#endif
}
void Service::detach(ConnectionState& connection)
{
    bool reserved=false;
    if (runtime::isValidController(connection.player))
    {
        auto& participant = participants_[connection.player];
        if (participant.occupied && participant.activeConnection == connection.serial)
        {
#ifdef PFME_HAS_PLAYABLE
            if(partyMode_ && partyStarted_[connection.player])party_->detach(participant.handle,connection.serial);
            else if(connection.player==playOwner_) runtime::releaseInputs();
#endif
            game_.disconnect(participant.handle);
            participant.activeConnection = 0;reserved=true;
        }
    }
    if(connection.authenticated&&!connection.verifiedPrincipal.empty()){
        for(const auto& p:participants_)if(p.occupied&&p.ownerPrincipal==connection.verifiedPrincipal)reserved=true;
        leases_.detach(connection.verifiedPrincipal,connection.serial,reserved,now_());
    }
    connection.player = INVALID_PLAYER_ID;
}
Reply Service::handle(ConnectionState& connection, std::string_view message)
{
    try
    {
        const auto now = now_();
        if (now - connection.window >= std::chrono::seconds(1))
        { connection.window = now; connection.commands = 0; }
        if (++connection.commands > COMMANDS_PER_SECOND)
            return {json::serialize(failure("", "RateLimited")), true};
        Request request;
        std::string error;
        if (!parseRequest(message, request, error)) return {json::serialize(failure(request.id, error)), false};
        if(serveMode_) {
            if(connection.verifiedPrincipal.empty())return {json::serialize(failure(request.id,"IdentityRequired")),true};
            uint64_t sequence=0;const auto parsed=std::from_chars(request.id.data(),request.id.data()+request.id.size(),sequence);
            if(request.id.empty()||request.id.front()=='0'||parsed.ec!=std::errc{}||parsed.ptr!=request.id.data()+request.id.size()||sequence<=connection.lastRequest)
                return {json::serialize(failure(request.id,"ReplayRejected")),true};
            connection.lastRequest=sequence;
            auto found=principalBudgets_.find(connection.verifiedPrincipal);
            if(found==principalBudgets_.end()) {
                if(principalBudgets_.size()>=32)return {json::serialize(failure(request.id,"Capacity")),true};
                found=principalBudgets_.emplace(connection.verifiedPrincipal,PrincipalBudget{}).first;
            }
            auto& budget=found->second;
            if(now-budget.window>=std::chrono::seconds(1)){budget.window=now;budget.requests=0;}
            if(++budget.requests>COMMANDS_PER_SECOND)return {json::serialize(failure(request.id,"RateLimited")),true};
        }
        ++requests_;auto result = dispatch(connection, request);
        if(result.at("ok").as_bool()){
            if(request.type=="reconnect")++reconnects_;
            if(request.type=="screen"){const auto& data=result.at("data").as_object();if(data.at("offset").as_uint64()==0){++frames_;unchanged_+=data.at("total").as_uint64()==0;}}
        }else{auto code=result.at("error").as_object().at("code").as_string();full_+=code=="ServerFull";authFailures_+=code=="AuthenticationFailed"; }
#ifdef PFME_HAS_PLAYABLE
        // Audio uses the existing authenticated reply channel and server-owned
        // cursor. A client cannot select a different listener or fetch history.
        if(partyMode_ && connection.authenticated && runtime::isValidController(connection.player) && partyStarted_[connection.player]) {
            const auto& participant=participants_[connection.player];runtime::AudioBatch batch;
            if(participant.activeConnection==connection.serial &&
               party_->audio(participant.handle,connection.serial,connection.audioCursor,!connection.audioReady,batch)) {
                json::array events;
                for(std::size_t i=0;i<batch.count;++i) {
                    const auto& event=batch.events[i];
                    events.push_back(json::array{std::to_string(event.sequence),event.at,event.frequency,event.duty});
                }
                result["audio"]=json::object{{"cursor",std::to_string(batch.cursor)},{"now_ms",batch.now},
                    {"frequency",batch.frequency},{"duty",batch.duty},{"reset",batch.reset},{"events",std::move(events)}};
                connection.audioCursor=batch.cursor;connection.audioReady=true;
            }
        }
#endif
        auto text = json::serialize(result);
        bytes_+=text.size();
        if (text.size() > MAX_RESPONSE_BYTES) return {json::serialize(failure(request.id, "ResponseLimit")), true};
        return {std::move(text), connection.authFailures >= AUTH_FAILURE_LIMIT};
    }
    catch (...)
    { return {json::serialize(failure("", "InternalError")), true}; }
}
json::object Service::dispatch(ConnectionState& connection, const Request& request)
{
    auto fail = [&](const std::string& code) { return failure(request.id, code); };
    auto ok = [&](json::object data = {}) { return response(request.id, request.type, std::move(data)); };
    if (request.type == "hello")
        return fieldsAre(request, {}) ? ok({{"authentication_required", !connection.authenticated}}) : fail("InvalidRequest");
    if (request.type == "authenticate")
    {
        if (connection.authenticated) return fail("AlreadyAuthenticated");
        if(serveMode_) {
            if(!fieldsAre(request,{})||connection.verifiedPrincipal.empty())return fail("AuthenticationFailed");
            expireReservations();
            if(!leases_.admit(connection.verifiedPrincipal,connection.serial)){lifecycle("SERVER_FULL",connection);return fail("ServerFull");}
            connection.identity=randomCredential();connection.authenticated=true;lifecycle("AUTHENTICATED",connection);
            return ok({{"authentication","serve"}});
        }
        std::string secret;
        SecretDigest candidate{};
        if (!fieldsAre(request, {"secret"}) || !textField(request, "secret", 64, secret) ||
            !credentialDigest(secret, candidate) || !sameDigest(candidate, gate_))
        { ++connection.authFailures; return fail("AuthenticationFailed"); }
        connection.identity = randomCredential();
        connection.authenticated = true;
        return ok({{"connection_id", connection.identity}});
    }
    if (!connection.authenticated) return fail("NotAuthenticated");
#ifdef PFME_HAS_PLAYABLE
#include "playable_service.inc"
    if((partyMode_ || runtime::isValidController(playOwner_)) && (request.type=="move" || request.type=="map" || request.type=="events"))
        return fail("RuntimeMode");
#endif
    if (request.type == "join")
    {
        if (runtime::isValidController(connection.player)) return fail("AlreadyBound");
        std::string name;
        if (!fieldsAre(request, {"name"}) || !textField(request, "name", NETWORK_PLAYER_NAME_SIZE - 1, name))
            return fail("InvalidRequest");
        if(serveMode_)for(const auto& p:participants_)
            if(p.occupied&&p.ownerPrincipal==connection.verifiedPrincipal)return fail("ResumeRequired");
        const std::string token = randomCredential();
        SecretDigest proof{};
        if (!credentialDigest(token, proof)) return fail("InternalError");
        PlayerHandle handle;
        const auto result = game_.join(name.c_str(), handle);
        if (result != SessionError::Ok) return fail(game_.banks()>1&&result==SessionError::Full?"ServerFull":sessionErrorName(result));
        SessionPlayer player;
        game_.player(handle, player);
        auto& participant = participants_[player.id];
        participant.occupied = true;
        participant.activeConnection = connection.serial;
        participant.handle = handle;
        participant.proof = proof;
        if(serveMode_)participant.ownerPrincipal=connection.verifiedPrincipal;
        connection.player = player.id;lifecycle("CHARACTER_BOUND",connection);
        if(serveMode_)return ok({{"player",playerJson(player)},{"ownership","verified-identity"}});
        return ok({{"player", playerJson(player)}, {"reconnect_token", token}});
    }
    if (request.type == "reconnect")
    {
        if (runtime::isValidController(connection.player)) return fail("AlreadyBound");
        if(serveMode_) {
            if(!fieldsAre(request,{}))return fail("OwnershipRequired");
            for(PlayerID id=0;id<runtime::NATIVE_CONTROLLER_CAPACITY;++id) {
                auto& p=participants_[id];if(!p.occupied||p.ownerPrincipal!=connection.verifiedPrincipal)continue;
                if(p.activeConnection){
#ifdef PFME_HAS_PLAYABLE
                    if(partyMode_&&partyStarted_[id])party_->detach(p.handle,p.activeConnection);
#endif
                    game_.disconnect(p.handle);p.activeConnection=0;
                }
                lifecycle("RECONNECTING",connection);
                const auto error=game_.reconnect(p.handle);if(error!=SessionError::Ok)return fail(sessionErrorName(error));
                p.activeConnection=connection.serial;connection.player=id;lifecycle("RECONNECTED",connection);
                SessionPlayer player;game_.player(p.handle,player);return ok({{"player",playerJson(player)}});
            }
            return fail("NoCharacter");
        }
        std::string token;
        SecretDigest proof{};
        if (!fieldsAre(request, {"token"}) || !textField(request, "token", 64, token) ||
            !credentialDigest(token, proof)) return fail("OwnershipRequired");
        PlayerID found = INVALID_PLAYER_ID;
        for (PlayerID i = 0; i < runtime::NATIVE_CONTROLLER_CAPACITY; ++i)
        {
            const bool match = sameDigest(participants_[i].proof, proof);
            if (match && participants_[i].occupied) found = i;
        }
        if (!runtime::isValidController(found)) return fail("OwnershipRequired");
        auto& participant = participants_[found];
        if (participant.activeConnection != 0) return fail("AlreadyConnected");
        const auto result = game_.reconnect(participant.handle);
        if (result != SessionError::Ok) return fail(sessionErrorName(result));
        participant.activeConnection = connection.serial;
        connection.player = found;
        SessionPlayer player;
        game_.player(participant.handle, player);
        return ok({{"player", playerJson(player)}});
    }
    if (request.type == "move" || request.type == "disconnect" || request.type == "leave")
    {
        if (!fieldsAre(request, request.type == "move" ?
                       std::initializer_list<std::string_view>{"direction"} : std::initializer_list<std::string_view>{}))
            return fail("InvalidRequest");
        if (!runtime::isValidController(connection.player)) return fail("NotBound");
        auto& participant = participants_[connection.player];
        if (participant.activeConnection != connection.serial) return fail("OwnershipRequired");
        if (request.type == "disconnect") { detach(connection); return ok(); }
        if (request.type == "leave")
        {
#ifdef PFME_HAS_PLAYABLE
            if(partyMode_ && partyStarted_[connection.player]) {
                if(!party_->leave(participant.handle,connection.serial))return fail("AdventureBusy");
                partyStarted_[connection.player]=false;partyScreen(connection.player).reset();
            } else
#endif
            {
                const auto result=game_.leave(participant.handle);
                if(result!=SessionError::Ok)return fail(sessionErrorName(result));
            }
#ifdef PFME_HAS_PLAYABLE
            if(connection.player==playOwner_) { runtime::releaseInputs(); playRetired_=true; }
#endif
            participant = Participant{};
            connection.player = INVALID_PLAYER_ID;
            return ok();
        }
        std::string direction;
        if (!textField(request, "direction", 5, direction)) return fail("InvalidDirection");
        Direction d;
        if (direction == "north") d = DIR_NORTH;
        else if (direction == "east") d = DIR_EAST;
        else if (direction == "south") d = DIR_SOUTH;
        else if (direction == "west") d = DIR_WEST;
        else return fail("InvalidDirection");
        const auto result = game_.move(participant.handle, d);
        if (result != SessionError::Ok) return fail(sessionErrorName(result));
        SessionPlayer player;
        game_.player(participant.handle, player);
        return ok({{"player", playerJson(player)}});
    }
    if (request.type == "players")
    {
        if (!fieldsAre(request, {})) return fail("InvalidRequest");
        json::array players;
#ifdef PFME_HAS_PLAYABLE
        if(partyMode_){
            if(!runtime::isValidController(connection.player))return fail("NotBound");
            PartySnapshot party;
            if(partyStarted_[connection.player]&&party_->status(participants_[connection.player].handle,connection.serial,party)){
                for(const auto& member:party.members)if(member.occupied)players.push_back(json::object{{"player_id",member.player},{"name",member.profile.displayName},{"connected",member.connected}});
            }else{SessionPlayer own;if(game_.player(participants_[connection.player].handle,own)==SessionError::Ok)players.push_back(playerJson(own));}
            return ok({{"players",std::move(players)}});
        }
#endif
        for (PlayerID i = 0; i < runtime::NATIVE_CONTROLLER_CAPACITY; ++i)
        { SessionPlayer player; if (game_.player(i, player) == SessionError::Ok) players.push_back(playerJson(player)); }
        return ok({{"players", std::move(players)}, {"event_cursor", std::to_string(game_.latestSequence())}});
    }
    if (request.type == "map")
    {
        if (!fieldsAre(request, {})) return fail("InvalidRequest");
        json::array rooms;
        const auto& topology = game_.dungeon();
        for (uint8_t i = 0; i < topology.roomCount; ++i)
        {
            const auto& room = topology.rooms[i];
            rooms.push_back(json::object{{"room", i}, {"north", roomJson(room.north)},
                {"east", roomJson(room.east)}, {"south", roomJson(room.south)}, {"west", roomJson(room.west)}});
        }
        return ok({{"navigation", "topology-only"}, {"rooms", std::move(rooms)}});
    }
    if (request.type == "events")
    {
        uint64_t cursor;
        if (!fieldsAre(request, {"since"}) || !cursorField(request, cursor)) return fail("InvalidRequest");
        SessionEvents events;
        const auto result = game_.eventsSince(cursor, events);
        if (result != SessionError::Ok && result != SessionError::HistoryGap) return fail(sessionErrorName(result));
        json::array items;
        for (std::size_t i = 0; i < events.count; ++i)
        {
            const auto& e = events.events[i];
            items.push_back(json::object{{"sequence", std::to_string(e.sequence)},
                {"type", sessionEventName(e.type)},
                {"player_id", runtime::isValidController(e.player) ? json::value(e.player) : json::value(nullptr)},
                {"from", roomJson(e.fromRoom)}, {"room", roomJson(e.room)}, {"text", e.text}});
        }
        auto reply = ok({{"oldest", std::to_string(events.oldest)}, {"newest", std::to_string(events.newest)},
            {"gap", events.gap}, {"events", std::move(items)}});
        if (events.gap) { reply["ok"] = false; reply["error"] = json::object{{"code", "HistoryGap"}}; }
        return reply;
    }
    if (request.type == "ping") return fieldsAre(request, {}) ? ok() : fail("InvalidRequest");
    return fail("UnknownCommand");
}
void Service::lifecycle(const char* event,const ConnectionState& c,const char* reason){
 json::object out{{"event",event},{"connection",c.serial},{"reason",reason}};
 if(!c.verifiedPrincipal.empty())out["account"]=std::to_string(std::hash<std::string>{}(c.verifiedPrincipal));
 if(runtime::isValidController(c.player))out["controller"]=c.player;
 std::clog<<json::serialize(out)<<'\n';
}
json::object Service::describe(ConnectionState& c){
 json::object out{{"connection",c.serial}};
 if(!c.verifiedPrincipal.empty())out["account"]=std::to_string(std::hash<std::string>{}(c.verifiedPrincipal));
 if(runtime::isValidController(c.player)){
  out["controller"]=c.player;
#ifdef PFME_HAS_PLAYABLE
  PartySnapshot p;if(partyMode_&&partyStarted_[c.player]&&party_->status(participants_[c.player].handle,c.serial,p)){
   out["party"]=std::to_string(p.id.high)+":"+std::to_string(p.id.low);out["player_id"]=runtime::nativePartyPlayerID(c.player);
  }
#endif
 }
 return out;
}
json::object Service::metrics()const{return {{"requests",requests_},{"reconnects",reconnects_},{"reconnect_expired",expirations_},{"server_full",full_},{"auth_failures",authFailures_},{"frames_requested",frames_},{"unchanged_frames",unchanged_},{"reply_bytes",bytes_},{"reserved_accounts",leases_.size()}};}
void Service::expireReservations(){
 const auto now=now_();for(auto& lease:leases_.entries)if(leases_.expired(lease,now)){
  bool safe=true;
  for(unsigned id=0;id<runtime::NATIVE_CONTROLLER_CAPACITY;++id){auto& p=participants_[id];if(!p.occupied||p.ownerPrincipal!=lease.principal)continue;
   if(p.activeConnection){safe=false;break;}
#ifdef PFME_HAS_PLAYABLE
   if(partyMode_&&partyStarted_[id]){if(!party_->expire(p.handle)){safe=false;break;}partyStarted_[id]=false;partyScreen(id).reset();}
   else
#endif
   if(game_.leave(p.handle)!=SessionError::Ok){safe=false;break;}
   ConnectionState expired;expired.player=id;expired.verifiedPrincipal=p.ownerPrincipal;lifecycle("RECONNECT_EXPIRED",expired);++expirations_;p=Participant{};
  }
  if(safe){principalBudgets_.erase(lease.principal);lease=AccountLeases::Entry{};}
 }
 for(auto it=principalBudgets_.begin();it!=principalBudgets_.end();)if(!leases_.find(it->first)&&now-it->second.window>=std::chrono::seconds(1))it=principalBudgets_.erase(it);else ++it;
}

}
