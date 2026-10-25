#ifndef PFME_SERVER_PROTOCOL_H
#define PFME_SERVER_PROTOCOL_H
#include <boost/json.hpp>
#include <initializer_list>
#include <string>
#include <string_view>
namespace pfme::server
{
namespace json = boost::json;
struct Request { std::string id, type; json::object fields; };
bool parseRequest(std::string_view text, Request& result, std::string& error);
bool fieldsAre(const Request& request, std::initializer_list<std::string_view> extra);
bool textField(const Request& request, const char* key, std::size_t maximum, std::string& text);
bool cursorField(const Request& request, uint64_t& sequence);
json::object response(const std::string& id, const std::string& type, json::object data = {});
json::object failure(const std::string& id, const std::string& code);
}
#endif
