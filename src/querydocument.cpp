#include "querydocument.h"
#include "json_helpers.h"
#include <iostream>

QueryDocument QueryDocument::fromJson(std::string_view jsonString, bool* ok)
{
    QueryDocument query;
    Json doc;
    if (!JsonHelpers::parse(jsonString, doc) || !doc.is_object())
    {
        std::cerr << "JSON is not an object\n";
        if (ok) *ok = false;
        return query;
    }

    query.from = JsonHelpers::int64Value(doc, "from");
    query.to = JsonHelpers::int64Value(doc, "to");
    query.doc = JsonHelpers::stringValue(doc, "doc");
    query.col = JsonHelpers::stringValue(doc, "col");
    query.limit = JsonHelpers::int64Value(doc, "limit");
    query.reverse = JsonHelpers::boolValue(doc, "reverse");
    query.where = JsonHelpers::stringValue(doc, "where");
    query.filter = JsonHelpers::stringValue(doc, "filter");

    if (ok) *ok = query.isValid();
    return query;
}

bool QueryDocument::isValid() const
{
    return to > 0 && from <= to && !doc.empty() && !col.empty();
} 
