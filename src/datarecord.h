#ifndef DATARECORD_H
#define DATARECORD_H

#include <cstdint>
#include <string>

struct DataRecord {
    std::int64_t timestamp = 0;
    std::string data;
    bool isNew = false;
};

#endif // DATARECORD_H 
