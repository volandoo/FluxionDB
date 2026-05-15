#include "querysessionsresponse.h"
#include "collection.h"

std::string QuerySessionsResponse::toString() const
{
    std::string out;
    out.append("{\"id\":\"");
    appendJsonEscapedUtf8(out, id);
    out.append("\",\"records\":{");
    bool first = true;
    for (const auto& [key, record] : records)
    {
        if (!first)
        {
            out.push_back(',');
        }
        first = false;
        out.push_back('"');
        appendJsonEscapedUtf8(out, key);
        out.append("\":{\"ts\":");
        out.append(std::to_string(record->timestamp));
        out.append(",\"data\":\"");
        appendJsonEscapedUtf8(out, record->data);
        out.append("\"}");
    }
    out.append("}}");
    return out;
}
