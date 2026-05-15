#ifndef DELETEMULTIPLERECORDS_H
#define DELETEMULTIPLERECORDS_H

#include <string_view>
#include <vector>
#include "deleterecord.h"

struct DeleteMultipleRecords {
    std::vector<DeleteRecord> records;
    static DeleteMultipleRecords fromJson(std::string_view jsonString, bool* ok);
    bool isValid() const;
};

#endif
