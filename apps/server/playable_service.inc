// Included inside Service::dispatch after authentication. No client state is authoritative.
if(request.type=="play" || request.type=="input" || request.type=="screen" || request.type=="party_status") {
    if(!isValidPlayerID(connection.player)) return fail("NotBound");
    auto& participant=participants_[connection.player];
    if(!participant.occupied || participant.activeConnection!=connection.serial) return fail("OwnershipRequired");
    if(partyMode_) {
        const auto id=connection.player;
        if(request.type=="party_status"){
            if(!fieldsAre(request,{}))return fail("InvalidRequest");
            PartySnapshot party;
            if(!partyStarted_[id] || !party_->status(participant.handle,connection.serial,party))return fail("NotInParty");
            json::array members;
            for(const auto& member:party.members)if(member.occupied)members.push_back(json::object{
                {"name",member.profile.displayName},{"class",partyCharacterClassName(member.profile.characterClass)},
                {"level",member.profile.level},{"leader",member.leader},{"connected",member.connected},{"location",partyLocationName(member.location)}});
            return ok({{"code",party.joinCode},{"activity",partyActivityName(party.activity)},{"locked",party.locked},{"members",std::move(members)}});
        }
        if(request.type=="play") {
            if(!fieldsAre(request,{}))return fail("InvalidRequest");
            if(!party_->start(participant.handle,connection.serial))return fail("RuntimeUnavailable");
            partyStarted_[id]=true;partyScreens_[id].reset();connection.audioReady=false;
            partyLastInput_[id]=std::chrono::steady_clock::now();
            return ok({{"mode","original-handheld-party"},{"width",240},{"height",240},{"capacity",MAX_MULTIPLAYER_PLAYERS},{"audio",true},{"persistence","process-memory-only"}});
        }
        const auto* pixels=party_->pixels(participant.handle,connection.serial);
        if(!partyStarted_[id] || !pixels)return fail("OwnershipRequired");
        if(request.type=="input") {
            std::string action;const auto* down=request.fields.if_contains("down");
            if(!fieldsAre(request,{"action","down"}) || !textField(request,"action",8,action) || !down || !down->is_bool())return fail("InvalidRequest");
            const bool textKey=action.size()==5 && action.compare(0,4,"key:")==0;
            if(action!="cw" && action!="ccw" && action!="encoder" && action!="a" && action!="b" && action!="release" && !textKey)return fail("InvalidAction");
            if(!party_->input(participant.handle,connection.serial,action,down->as_bool()))return fail("InvalidAction");
            partyLastInput_[id]=std::chrono::steady_clock::now();return ok();
        }
        uint64_t since;const auto* offset=request.fields.if_contains("offset");
        if(!fieldsAre(request,{"since","offset"}) || !cursorField(request,since) || !offset ||
           !(offset->is_uint64() || (offset->is_int64() && offset->as_int64()>=0)))return fail("InvalidRequest");
        const uint64_t position=offset->is_uint64()?offset->as_uint64():static_cast<uint64_t>(offset->as_int64());
        if(position>230400)return fail("InvalidCursor");
        json::object frame;
        if(!partyScreens_[id].next(pixels,since,static_cast<size_t>(position),frame))return fail("InvalidCursor");
        frame["text_input"]=runtime::textInputActive();
        return ok(std::move(frame));
    }
    if(request.type=="party_status")return fail("RuntimeMode");
    if(request.type=="play") {
        if(!fieldsAre(request,{})) return fail("InvalidRequest");
        if(playRetired_) return fail("RuntimeRetired");
        if(isValidPlayerID(playOwner_) && playOwner_!=connection.player) return fail("RuntimeOccupied");
        if(!isValidPlayerID(playOwner_)) {
            runtime::initialize(seed_); playOwner_=connection.player;
        }
        screen_.reset(); runtime::releaseInputs();
        return ok({{"mode","original-handheld-single-controller"},{"width",240},{"height",240},{"persistence","process-memory-only"}});
    }
    if(playRetired_ || playOwner_!=connection.player) return fail("OwnershipRequired");
    if(request.type=="input") {
        std::string action;
        const auto* down=request.fields.if_contains("down");
        if(!fieldsAre(request,{"action","down"}) || !textField(request,"action",8,action) || !down || !down->is_bool()) return fail("InvalidRequest");
        const bool textKey=action.size()==5 && action.compare(0,4,"key:")==0;
        if(action!="cw" && action!="ccw" && action!="encoder" && action!="a" && action!="b" && action!="release" && !textKey) return fail("InvalidAction");
        if(!runtime::input(action,down->as_bool())) return fail("InvalidAction");
        lastInput_=std::chrono::steady_clock::now();
        return ok();
    }
    uint64_t since;
    const auto* offset=request.fields.if_contains("offset");
    if(!fieldsAre(request,{"since","offset"}) || !cursorField(request,since) || !offset ||
       !(offset->is_uint64() || (offset->is_int64() && offset->as_int64()>=0))) return fail("InvalidRequest");
    const uint64_t position=offset->is_uint64()?offset->as_uint64():static_cast<uint64_t>(offset->as_int64());
    if(position>230400) return fail("InvalidCursor");
    json::object frame;
    if(!screen_.next(runtime::pixels(),since,static_cast<size_t>(position),frame)) return fail("InvalidCursor");
    frame["text_input"]=runtime::textInputActive();
    return ok(std::move(frame));
}
