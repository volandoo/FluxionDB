#include "keyvalue.h"
#include "json_helpers.h"

KeyValue KeyValue::fromJson(std::string_view jsonString, bool* ok)
{
    KeyValue kv;
    Json doc;
    if (!JsonHelpers::parse(jsonString, doc) || !doc.is_object()) {
        if (ok) *ok = false;
        return kv;
    }
    kv.key = JsonHelpers::stringValue(doc, "key");
    kv.value = JsonHelpers::stringValue(doc, "value");
    kv.col = JsonHelpers::stringValue(doc, "col");
    if (ok) *ok = kv.isValid();
    return kv;
}

bool KeyValue::isValid() const
{
    return !col.empty();
}

bool KeyValue::hasKey() const
{
    return !key.empty();
}

bool KeyValue::hasValue() const
{
    return !value.empty();
}
