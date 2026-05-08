#ifndef QUERYDOCUMENT_H
#define QUERYDOCUMENT_H

#include <QString>

struct QueryDocument {
    qint64 from;
    qint64 to;
    qint64 limit;
    bool reverse;
    QString doc;
    QString col;
    QString where;
    QString filter;

    
    static QueryDocument fromJson(const QString& jsonString, bool* ok = nullptr);
    bool isValid() const;
};

#endif // QUERYDOCUMENT_H 
