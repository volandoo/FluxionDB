#include "querysessions.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>

QuerySessions QuerySessions::fromJson(const QString& jsonString, bool* ok)
{
    QuerySessions payload;
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << error.errorString();
        if (ok) *ok = false;
        return payload;
    }

    if (!doc.isObject()) {
        qWarning() << "JSON is not an object";
        if (ok) *ok = false;
        return payload;
    }

    QJsonObject obj = doc.object();
    
    payload.ts = obj["ts"].toVariant().toLongLong();
    payload.from = obj["from"].toVariant().toLongLong();
    payload.doc = obj["doc"].toString();
    payload.col = obj["col"].toString();
    payload.where = obj["where"].toString();
    payload.filter = obj["filter"].toString();


    if (ok) *ok = payload.isValid();
    return payload;
}

bool QuerySessions::isValid() const
{
    return ts > 0 && !col.isEmpty();
} 
