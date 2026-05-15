#ifndef QUERYDOCUMENT_H
#define QUERYDOCUMENT_H

#include <cstdint>
#include <string>
#include <string_view>

struct QueryDocument {
    std::int64_t from = 0;
    std::int64_t to = 0;
    std::int64_t limit = 0;
    bool reverse = false;
    std::string doc;
    std::string col;
    std::string where;
    std::string filter;

    
    static QueryDocument fromJson(std::string_view jsonString, bool* ok = nullptr);
    bool isValid() const;
};

#endif // QUERYDOCUMENT_H 
