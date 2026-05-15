#include "deletedocument.h"
#include "json_helpers.h"
#include <iostream>

DeleteDocument DeleteDocument::fromJson(std::string_view jsonString, bool* ok)
{
    DeleteDocument query;
    Json doc;
    if (!JsonHelpers::parse(jsonString, doc) || !doc.is_object())
    {
        std::cerr << "JSON is not an object\n";
        if (ok) *ok = false;
        return query;
    }

    query.doc = JsonHelpers::stringValue(doc, "doc");
    query.col = JsonHelpers::stringValue(doc, "col");

    if (ok) *ok = query.isValid();
    return query;
}

bool DeleteDocument::isValid() const
{
    return !doc.empty();
} 
