// Included inside Service::dispatch after authentication. No client state is authoritative.
if(request.type=="play" || request.type=="input" || request.type=="screen") {
    if(!isValidPlayerID(connection.player)) return fail("NotBound");
    auto& participant=participants_[connection.player];
    if(!participant.occupied || participant.activeConnection!=connection.serial) return fail("OwnershipRequired");
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
        if(action!="cw" && action!="ccw" && action!="encoder" && action!="a" && action!="b" && action!="release") return fail("InvalidAction");
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
    return ok(std::move(frame));
}
