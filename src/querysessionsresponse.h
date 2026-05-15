#ifndef QUERYSESSIONSRESPONSE_H
#define QUERYSESSIONSRESPONSE_H

#include <string>
#include <unordered_map>
#include "datarecord.h"

struct QuerySessionsResponse {
    std::string id;
    std::unordered_map<std::string, DataRecord*> records;

    std::string toString() const;
};

#endif // QUERYSESSIONSRESPONSE_H 
