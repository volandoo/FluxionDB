#include "deletemultiplerecords.h"
#include "json_helpers.h"
#include <iostream>

DeleteMultipleRecords DeleteMultipleRecords::fromJson(std::string_view jsonString, bool* ok) {
    DeleteMultipleRecords query;
    Json doc;
    if (!JsonHelpers::parse(jsonString, doc) || !doc.is_array())
    {
        std::cerr << "JSON is not an array\n";
        if (ok) *ok = false;
        return query;
    }

    query.records.reserve(doc.size());
    for (const Json& value : doc) {
        if (!value.is_object()) {
            std::cerr << "JSON is not an object\n";
            if (ok) *ok = false;
            return query;
        }
        bool recordOk = false;
        DeleteRecord record = DeleteRecord::fromJsonObject(value, &recordOk);
        if (!recordOk || !record.isValid()) {
            std::cerr << "Invalid delete record object\n";
            if (ok) *ok = false;
            return query;
        }
        query.records.push_back(std::move(record));
    }
    if (ok) *ok = true;
    return query;
}

bool DeleteMultipleRecords::isValid() const {
    return !records.empty();
}
