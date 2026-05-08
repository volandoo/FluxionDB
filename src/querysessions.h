#ifndef QUERYSESSIONS_H
#define QUERYSESSIONS_H

#include <QString>

struct QuerySessions {
    qint64 ts;
    qint64 from;
    QString doc;
    QString col;
    QString where;
    QString filter;
    
    static QuerySessions fromJson(const QString& jsonString, bool* ok = nullptr);
    bool isValid() const;
};

#endif // QUERYSESSIONS_H 
