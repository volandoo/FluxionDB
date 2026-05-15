#include "querysessions.h"
#include "json_helpers.h"
#include <iostream>

QuerySessions QuerySessions::fromJson(std::string_view jsonString, bool* ok)
{
    QuerySessions payload;
    Json doc;
    if (!JsonHelpers::parse(jsonString, doc) || !doc.is_object())
    {
        std::cerr << "JSON is not an object\n";
        if (ok) *ok = false;
        return payload;
    }

    payload.ts = JsonHelpers::int64Value(doc, "ts");
    payload.from = JsonHelpers::int64Value(doc, "from");
    payload.doc = JsonHelpers::stringValue(doc, "doc");
    payload.col = JsonHelpers::stringValue(doc, "col");
    payload.where = JsonHelpers::stringValue(doc, "where");
    payload.filter = JsonHelpers::stringValue(doc, "filter");

    if (ok) *ok = payload.isValid();
    return payload;
}

bool QuerySessions::isValid() const
{
    return ts > 0 && !col.empty();
} 
