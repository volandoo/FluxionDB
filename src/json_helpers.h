#ifndef JSON_HELPERS_H
#define JSON_HELPERS_H

#include <cstdint>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

using Json = nlohmann::json;

namespace JsonHelpers {

inline bool parse(std::string_view text, Json& out)
{
    try
    {
        out = Json::parse(text.begin(), text.end());
        return true;
    }
    catch (const Json::parse_error& error)
    {
        std::cerr << "JSON parse error: " << error.what() << '\n';
        return false;
    }
}

inline std::string stringValue(const Json& obj, const char* key)
{
    const auto it = obj.find(key);
    return it != obj.end() && it->is_string() ? it->get<std::string>() : std::string();
}

inline std::int64_t int64Value(const Json& obj, const char* key)
{
    const auto it = obj.find(key);
    if (it == obj.end())
    {
        return 0;
    }
    if (it->is_number_integer() || it->is_number_unsigned())
    {
        return it->get<std::int64_t>();
    }
    if (it->is_number_float())
    {
        return static_cast<std::int64_t>(it->get<double>());
    }
    if (it->is_string())
    {
        try
        {
            return std::stoll(it->get<std::string>());
        }
        catch (...)
        {
            return 0;
        }
    }
    return 0;
}

inline bool boolValue(const Json& obj, const char* key)
{
    const auto it = obj.find(key);
    if (it == obj.end())
    {
        return false;
    }
    if (it->is_boolean())
    {
        return it->get<bool>();
    }
    if (it->is_number_integer())
    {
        return it->get<int>() != 0;
    }
    return false;
}

inline std::string compactObjectWithId(std::string_view id)
{
    Json obj;
    obj["id"] = id;
    return obj.dump();
}

} // namespace JsonHelpers

#endif // JSON_HELPERS_H
