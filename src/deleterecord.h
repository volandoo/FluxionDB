#ifndef DELETERECORD_H
#define DELETERECORD_H

#include <cstdint>
#include <string>
#include <string_view>
#include "json_helpers.h"

struct DeleteRecord {
    std::string doc;
    std::string col;
    std::int64_t ts = 0;
    
    static DeleteRecord fromJson(std::string_view jsonString, bool* ok = nullptr);
    static DeleteRecord fromJsonObject(const Json& jsonObject, bool* ok = nullptr);
    bool isValid() const;
};

#endif // DELETERECORD_H
