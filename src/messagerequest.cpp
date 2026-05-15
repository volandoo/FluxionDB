#include "messagerequest.h"
#include "json_helpers.h"
#include <iostream>

MessageRequest MessageRequest::fromJson(std::string_view jsonString, bool* ok)
{
    MessageRequest msg;
    Json doc;
    if (!JsonHelpers::parse(jsonString, doc) || !doc.is_object())
    {
        std::cerr << "JSON is not an object\n";
        if (ok) *ok = false;
        return msg;
    }

    msg.id = JsonHelpers::stringValue(doc, "id");
    msg.type = JsonHelpers::stringValue(doc, "type");
    msg.data = JsonHelpers::stringValue(doc, "data");

    if (ok) *ok = msg.isValid();
    return msg;
}

bool MessageRequest::isValid() const
{
    return !id.empty() && !type.empty() && !data.empty();
}
