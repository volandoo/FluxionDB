#ifndef MESSAGEREQUEST_H
#define MESSAGEREQUEST_H

#include <string>
#include <string_view>

struct MessageRequest {
    std::string id;
    std::string type;
    std::string data;
    
    static MessageRequest fromJson(std::string_view jsonString, bool* ok = nullptr);
    bool isValid() const;
};

#endif // MESSAGEREQUEST_H 
