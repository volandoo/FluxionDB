#ifndef KEYVALUE_H
#define KEYVALUE_H

#include <string>
#include <string_view>

struct KeyValue {
    std::string key;
    std::string value;
    std::string col;
    
    static KeyValue fromJson(std::string_view jsonString, bool* ok = nullptr);
    bool isValid() const;
    bool hasValue() const;
    bool hasKey() const;
};

#endif // KEYVALUE_H 
