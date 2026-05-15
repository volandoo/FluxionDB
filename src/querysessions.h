#ifndef QUERYSESSIONS_H
#define QUERYSESSIONS_H

#include <cstdint>
#include <string>
#include <string_view>

struct QuerySessions {
    std::int64_t ts = 0;
    std::int64_t from = 0;
    std::string doc;
    std::string col;
    std::string where;
    std::string filter;
    
    static QuerySessions fromJson(std::string_view jsonString, bool* ok = nullptr);
    bool isValid() const;
};

#endif // QUERYSESSIONS_H 
