#ifndef QUERYDELETECOLLECTION_H
#define QUERYDELETECOLLECTION_H

#include <string>
#include <string_view>

struct DeleteCollection {
    std::string col;
    
    static DeleteCollection fromJson(std::string_view jsonString, bool* ok = nullptr);
    bool isValid() const;
};

#endif // QUERYDELETECOLLECTION_H 
