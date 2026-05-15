#include "insertrequest.h"
#include "json_helpers.h"
#include <iostream>

std::vector<InsertRequest> InsertRequest::fromJson(std::string_view jsonString, bool* ok)
{       
    std::vector<InsertRequest> payloads;
    Json doc;
    if (!JsonHelpers::parse(jsonString, doc) || !doc.is_array())
    {
        std::cerr << "JSON is not an array\n";
        if (ok) *ok = false;
        return payloads;
    }

    payloads.reserve(doc.size());
    for (const Json& value : doc)
    {
        if (!value.is_object())
        {
            std::cerr << "JSON is not an object\n";
            if (ok) *ok = false;
            return payloads;
        }
    
        InsertRequest payload;
        payload.ts = JsonHelpers::int64Value(value, "ts");
        payload.doc = JsonHelpers::stringValue(value, "doc");
        payload.data = JsonHelpers::stringValue(value, "data");
        payload.col = JsonHelpers::stringValue(value, "col");
        payloads.push_back(std::move(payload));
    }

    if (ok) *ok = true;
    return payloads;
}

bool InsertRequest::isValid() const
{
    return ts > 0 && !doc.empty() && !data.empty() && !col.empty();
} 
