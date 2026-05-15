#ifndef DELETE_DOCUMENT_H
#define DELETE_DOCUMENT_H

#include <string>
#include <string_view>

struct DeleteDocument {
    std::string doc;
    std::string col;
    
    static DeleteDocument fromJson(std::string_view jsonString, bool* ok = nullptr);
    bool isValid() const;
};

#endif // DELETE_DOCUMENT_H 
