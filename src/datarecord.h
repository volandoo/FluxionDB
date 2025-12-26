#ifndef DATARECORD_H
#define DATARECORD_H

#include <QString>
#include <QJsonObject>
#include <string>

struct DataRecord {
    qint64 timestamp;
    // using std::string for data since it's half the size of QString
    std::string data;
    bool isNew;
    
    QJsonObject toJson() const;
    QString toString() const;

};

#endif // DATARECORD_H 
