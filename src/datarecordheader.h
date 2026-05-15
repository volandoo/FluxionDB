#ifndef DATARECORDHEADER_H
#define DATARECORDHEADER_H

#include <vector>
#include "datarecord.h"

struct DataRecordHeader {
    std::vector<DataRecord*> records;
    bool hasChanged = false;
};

#endif // DATARECORDHEADER_H 
