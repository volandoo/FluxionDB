#ifndef DELETERECORDSRANGE_H
#define DELETERECORDSRANGE_H

#include <cstdint>
#include <string>
#include <string_view>
#include "json_helpers.h"

struct DeleteRecordsRange {
    std::string doc;
    std::string col;
    std::int64_t fromTs = 0;
    std::int64_t toTs = 0;
    
    static DeleteRecordsRange fromJson(std::string_view jsonString, bool* ok = nullptr);
    static DeleteRecordsRange fromJsonObject(const Json& jsonObject, bool* ok = nullptr);
    bool isValid() const;
};

#endif // DELETERECORDSRANGE_H
