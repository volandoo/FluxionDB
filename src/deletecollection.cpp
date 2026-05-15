#include "deletecollection.h"
#include "json_helpers.h"
#include <iostream>

DeleteCollection DeleteCollection::fromJson(std::string_view jsonString, bool* ok)
{
    DeleteCollection query;
    Json doc;
    if (!JsonHelpers::parse(jsonString, doc) || !doc.is_object())
    {
        std::cerr << "JSON is not an object\n";
        if (ok) *ok = false;
        return query;
    }

    query.col = JsonHelpers::stringValue(doc, "col");
    if (ok) *ok = query.isValid();
    return query;
}

bool DeleteCollection::isValid() const
{
    return !col.empty();
} 
