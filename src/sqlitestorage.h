#ifndef SQLITESTORAGE_H
#define SQLITESTORAGE_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QSqlDatabase>

class QSqlQuery;

class SqliteStorage
{
public:
    struct StoredRecord
    {
        QString document;
        qint64 timestamp;
        QString data;
    };

    struct StoredKeyValue
    {
        QString key;
        QString value;
    };

    SqliteStorage();
    ~SqliteStorage();

    bool initialize(const QString &dataFolder);
    void shutdown();
    bool isOpen() const;

    QStringList collections() const;
    QVector<StoredRecord> fetchRecords(const QString &collection) const;
    QVector<StoredKeyValue> fetchKeyValues(const QString &collection) const;

    bool upsertRecord(const QString &collection, const QString &document, qint64 timestamp, const QString &data);
    bool deleteRecord(const QString &collection, const QString &document, qint64 timestamp);
    bool deleteRecordsInRange(const QString &collection, const QString &document, qint64 fromTs, qint64 toTs);
    bool deleteDocument(const QString &collection, const QString &document);
    bool deleteCollection(const QString &collection);
    bool upsertKeyValue(const QString &collection, const QString &key, const QString &value);
    bool removeKeyValue(const QString &collection, const QString &key);

    struct ApiKeyRow
    {
        QString key;
        QString scope;
        bool deletable;
    };
    bool upsertApiKey(const QString &key, const QString &scope, bool deletable);
    bool deleteApiKey(const QString &key);
    QVector<ApiKeyRow> fetchApiKeys() const;
    bool beginTransaction();
    bool commitTransaction();
    void rollbackTransaction();

private:
    bool ensureSchema();
    void logError(const QSqlQuery &query, const QString &context) const;

    QString m_connectionName;
    mutable QSqlDatabase m_db;
    bool m_isOpen;
    QString m_dbFilePath;
};

#endif // SQLITESTORAGE_H
