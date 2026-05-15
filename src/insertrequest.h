#ifndef INSERTREQUEST_H
#define INSERTREQUEST_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct InsertRequest {
    std::int64_t ts = 0;
    std::string doc;
    std::string data;
    std::string col;
    
    static std::vector<InsertRequest> fromJson(std::string_view jsonString, bool* ok = nullptr);
    bool isValid() const;
};

#endif // INSERTREQUEST_H 
