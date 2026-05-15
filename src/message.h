#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <string_view>

struct Message {
    std::string id;
    std::string type;
    std::string data;
    
    static Message fromJson(std::string_view jsonString, bool* ok = nullptr);
    bool isValid() const;
};

#endif // MESSAGE_H 
