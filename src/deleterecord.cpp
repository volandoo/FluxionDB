
#include "deleterecord.h"
#include <iostream>

DeleteRecord DeleteRecord::fromJsonObject(const Json& jsonObject, bool* ok) {

    DeleteRecord query;
    if (!jsonObject.is_object())
    {
        if (ok) *ok = false;
        return query;
    }
    query.doc = JsonHelpers::stringValue(jsonObject, "doc");
    query.col = JsonHelpers::stringValue(jsonObject, "col");
    query.ts = JsonHelpers::int64Value(jsonObject, "ts");
    if (ok) *ok = true;
    return query;
}

DeleteRecord DeleteRecord::fromJson(std::string_view jsonString, bool* ok) {

    DeleteRecord query;
    Json doc;
    if (!JsonHelpers::parse(jsonString, doc) || !doc.is_object())
    {
        std::cerr << "JSON is not an object\n";
        if (ok) *ok = false;
        return query;
    }

    return fromJsonObject(doc, ok);

}
bool DeleteRecord::isValid() const {
    if (doc.empty()) {
        std::cerr << "doc is empty\n";
        return false;
    }
    if (col.empty()) {
        std::cerr << "col is empty\n";
        return false;
    }
    if (ts <= 0) {
        std::cerr << "ts is not positive\n";
        return false;
    }
    return true;
}
