#include "protocol.h"
#include "server_limits.h"
#include <limits>

namespace pfme::server
{
bool textField(const Request& request, const char* key, std::size_t maximum, std::string& text)
{
    const auto* value = request.fields.if_contains(key);
    if (!value || !value->is_string() || value->as_string().empty() ||
        value->as_string().size() > maximum) return false;
    text.assign(value->as_string().data(), value->as_string().size());
    for (unsigned char c : text) if (c < 33 || c > 126) return false;
    return true;
}
bool parseRequest(std::string_view text, Request& result, std::string& error)
{
    result = Request{};
    if (text.size() > MAX_REQUEST_BYTES) { error = "MessageTooLarge"; return false; }
    json::parse_options options;
    options.max_depth = MAX_JSON_DEPTH;
    boost::system::error_code ec;
    auto value = json::parse(text, ec, {}, options);
    if (ec || !value.is_object()) { error = "MalformedJson"; return false; }
    result.fields = std::move(value.as_object());
    if (!textField(result, "id", 32, result.id) || !textField(result, "type", 24, result.type))
    { result.id.clear(); error = "InvalidRequest"; return false; }
    const auto* version = result.fields.if_contains("v");
    if (!version || !((version->is_int64() && version->as_int64() == PROTOCOL_VERSION) ||
                     (version->is_uint64() && version->as_uint64() == PROTOCOL_VERSION)))
    { error = "UnsupportedVersion"; return false; }
    return true;
}
bool fieldsAre(const Request& request, std::initializer_list<std::string_view> extra)
{
    for (const auto& field : request.fields)
    {
        const std::string_view key(field.key().data(), field.key().size());
        if (key == "id" || key == "v" || key == "type") continue;
        bool known = false;
        for (auto allowed : extra) if (key == allowed) known = true;
        if (!known) return false;
    }
    return true;
}
bool cursorField(const Request& request, uint64_t& sequence)
{
    std::string text;
    if (!textField(request, "since", 20, text)) return false;
    sequence = 0;
    for (char c : text)
    {
        if (c < '0' || c > '9') return false;
        const unsigned digit = static_cast<unsigned>(c - '0');
        if (sequence > (std::numeric_limits<uint64_t>::max() - digit) / 10) return false;
        sequence = sequence * 10 + digit;
    }
    return true;
}
json::object response(const std::string& id, const std::string& type, json::object data)
{ return {{"v", PROTOCOL_VERSION}, {"id", id}, {"type", type}, {"ok", true}, {"data", std::move(data)}}; }
json::object failure(const std::string& id, const std::string& code)
{ return {{"v", PROTOCOL_VERSION}, {"id", id}, {"type", "error"}, {"ok", false}, {"error", {{"code", code}}}}; }
}
