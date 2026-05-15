#include "deleterecordsrange.h"
#include <iostream>

DeleteRecordsRange DeleteRecordsRange::fromJsonObject(const Json& jsonObject, bool* ok) {
    DeleteRecordsRange query;
    if (!jsonObject.is_object())
    {
        if (ok) *ok = false;
        return query;
    }
    query.doc = JsonHelpers::stringValue(jsonObject, "doc");
    query.col = JsonHelpers::stringValue(jsonObject, "col");
    query.fromTs = JsonHelpers::int64Value(jsonObject, "fromTs");
    query.toTs = JsonHelpers::int64Value(jsonObject, "toTs");
    if (ok) *ok = true;
    return query;
}

DeleteRecordsRange DeleteRecordsRange::fromJson(std::string_view jsonString, bool* ok) {
    DeleteRecordsRange query;
    Json doc;
    if (!JsonHelpers::parse(jsonString, doc) || !doc.is_object())
    {
        std::cerr << "JSON is not an object\n";
        if (ok) *ok = false;
        return query;
    }

    return fromJsonObject(doc, ok);
}

bool DeleteRecordsRange::isValid() const {
    if (doc.empty()) {
        std::cerr << "doc is empty\n";
        return false;
    }
    if (col.empty()) {
        std::cerr << "col is empty\n";
        return false;
    }
    if (fromTs <= 0) {
        std::cerr << "fromTs is not positive\n";
        return false;
    }
    if (toTs <= 0) {
        std::cerr << "toTs is not positive\n";
        return false;
    }
    if (fromTs > toTs) {
        std::cerr << "fromTs is greater than toTs\n";
        return false;
    }
    return true;
}
