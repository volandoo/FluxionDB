#include "sqlitestorage.h"

#include <QDebug>
#include <QDir>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <algorithm>

SqliteStorage::SqliteStorage() : m_isOpen(false)
{
}

SqliteStorage::~SqliteStorage()
{
    shutdown();
}

bool SqliteStorage::initialize(const QString &dataFolder)
{
    if (dataFolder.isEmpty())
    {
        qWarning() << "SQLite storage disabled because data folder is empty";
        return false;
    }

    if (m_isOpen)
    {
        return true;
    }

    QDir dir(dataFolder);
    if (!dir.exists() && !QDir().mkpath(dataFolder))
    {
        qWarning() << "Failed to create data directory for SQLite storage:" << dataFolder;
        return false;
    }

    m_connectionName = QStringLiteral("fluxiondb_%1").arg(reinterpret_cast<qulonglong>(this));
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_dbFilePath = dir.absoluteFilePath(QStringLiteral("fluxion.db"));
    m_db.setDatabaseName(m_dbFilePath);

    if (!m_db.open())
    {
        qCritical() << "Failed to open SQLite database:" << m_db.lastError().text();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
        return false;
    }

    m_isOpen = true;
    if (!ensureSchema())
    {
        shutdown();
        return false;
    }

    qInfo() << "SQLite storage initialized at" << m_dbFilePath;
    return true;
}

void SqliteStorage::shutdown()
{
    if (!m_isOpen)
    {
        return;
    }

    if (m_db.isValid())
    {
        m_db.close();
    }

    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
    m_connectionName.clear();
    m_isOpen = false;
}

bool SqliteStorage::isOpen() const
{
    return m_isOpen;
}

QStringList SqliteStorage::collections() const
{
    QStringList result;
    if (!m_isOpen)
    {
        return result;
    }

    QSet<QString> names;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT DISTINCT collection FROM records")))
    {
        while (query.next())
        {
            names.insert(query.value(0).toString());
        }
    }

    if (query.exec(QStringLiteral("SELECT DISTINCT collection FROM key_values")))
    {
        while (query.next())
        {
            names.insert(query.value(0).toString());
        }
    }

    result = names.values();
    std::sort(result.begin(), result.end());
    return result;
}

QVector<SqliteStorage::StoredRecord> SqliteStorage::fetchRecords(const QString &collection) const
{
    QVector<StoredRecord> rows;
    if (!m_isOpen)
    {
        return rows;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT doc, ts, data FROM records WHERE collection = ? ORDER BY ts ASC"));
    query.addBindValue(collection);
    if (!query.exec())
    {
        logError(query, QStringLiteral("Failed to fetch records"));
        return rows;
    }

    while (query.next())
    {
        StoredRecord record;
        record.document = query.value(0).toString();
        record.timestamp = query.value(1).toLongLong();
        record.data = query.value(2).toString();
        rows.append(record);
    }
    return rows;
}

QVector<SqliteStorage::StoredKeyValue> SqliteStorage::fetchKeyValues(const QString &collection) const
{
    QVector<StoredKeyValue> rows;
    if (!m_isOpen)
    {
        return rows;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT key, value FROM key_values WHERE collection = ?"));
    query.addBindValue(collection);
    if (!query.exec())
    {
        logError(query, QStringLiteral("Failed to fetch key values"));
        return rows;
    }

    while (query.next())
    {
        StoredKeyValue kv;
        kv.key = query.value(0).toString();
        kv.value = query.value(1).toString();
        rows.append(kv);
    }
    return rows;
}

bool SqliteStorage::upsertRecord(const QString &collection, const QString &document, qint64 timestamp, const QString &data)
{
    if (!m_isOpen)
    {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO records (ts, collection, doc, data) VALUES (?, ?, ?, ?)"));
    query.addBindValue(timestamp);
    query.addBindValue(collection);
    query.addBindValue(document);
    query.addBindValue(data);
    if (!query.exec())
    {
        logError(query, QStringLiteral("Failed to upsert record"));
        return false;
    }
    return true;
}

bool SqliteStorage::deleteRecord(const QString &collection, const QString &document, qint64 timestamp)
{
    if (!m_isOpen)
    {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM records WHERE ts = ? AND collection = ? AND doc = ?"));
    query.addBindValue(timestamp);
    query.addBindValue(collection);
    query.addBindValue(document);
    if (!query.exec())
    {
        logError(query, QStringLiteral("Failed to delete record"));
        return false;
    }
    return true;
}

bool SqliteStorage::deleteRecordsInRange(const QString &collection, const QString &document, qint64 fromTs, qint64 toTs)
{
    if (!m_isOpen)
    {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM records WHERE collection = ? AND doc = ? AND ts >= ? AND ts <= ?"));
    query.addBindValue(collection);
    query.addBindValue(document);
    query.addBindValue(fromTs);
    query.addBindValue(toTs);
    if (!query.exec())
    {
        logError(query, QStringLiteral("Failed to delete records in range"));
        return false;
    }
    return true;
}

bool SqliteStorage::deleteDocument(const QString &collection, const QString &document)
{
    if (!m_isOpen)
    {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM records WHERE collection = ? AND doc = ?"));
    query.addBindValue(collection);
    query.addBindValue(document);
    if (!query.exec())
    {
        logError(query, QStringLiteral("Failed to delete document"));
        return false;
    }
    return true;
}

bool SqliteStorage::deleteCollection(const QString &collection)
{
    if (!m_isOpen)
    {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM records WHERE collection = ?"));
    query.addBindValue(collection);
    if (!query.exec())
    {
        logError(query, QStringLiteral("Failed to delete collection records"));
        return false;
    }

    QSqlQuery kvQuery(m_db);
    kvQuery.prepare(QStringLiteral("DELETE FROM key_values WHERE collection = ?"));
    kvQuery.addBindValue(collection);
    if (!kvQuery.exec())
    {
        logError(kvQuery, QStringLiteral("Failed to delete collection key values"));
        return false;
    }
    return true;
}

bool SqliteStorage::upsertKeyValue(const QString &collection, const QString &key, const QString &value)
{
    if (!m_isOpen)
    {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO key_values (collection, key, value) VALUES (?, ?, ?)"));
    query.addBindValue(collection);
    query.addBindValue(key);
    query.addBindValue(value);
    if (!query.exec())
    {
        logError(query, QStringLiteral("Failed to upsert key value"));
        return false;
    }
    return true;
}

bool SqliteStorage::removeKeyValue(const QString &collection, const QString &key)
{
    if (!m_isOpen)
    {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM key_values WHERE collection = ? AND key = ?"));
    query.addBindValue(collection);
    query.addBindValue(key);
    if (!query.exec())
    {
        logError(query, QStringLiteral("Failed to remove key value"));
        return false;
    }
    return true;
}

bool SqliteStorage::upsertApiKey(const QString &key, const QString &scope, bool deletable)
{
    if (!m_isOpen)
    {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO api_keys (api_key, scope, deletable) VALUES (?, ?, ?)"));
    query.addBindValue(key);
    query.addBindValue(scope);
    query.addBindValue(deletable);
    if (!query.exec())
    {
        logError(query, QStringLiteral("Failed to upsert API key"));
        return false;
    }
    return true;
}

bool SqliteStorage::deleteApiKey(const QString &key)
{
    if (!m_isOpen)
    {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM api_keys WHERE api_key = ?"));
    query.addBindValue(key);
    if (!query.exec())
    {
        logError(query, QStringLiteral("Failed to delete API key"));
        return false;
    }
    return true;
}

QVector<SqliteStorage::ApiKeyRow> SqliteStorage::fetchApiKeys() const
{
    QVector<ApiKeyRow> rows;
    if (!m_isOpen)
    {
        return rows;
    }
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT api_key, scope, deletable FROM api_keys")))
    {
        logError(query, QStringLiteral("Failed to fetch API keys"));
        return rows;
    }
    while (query.next())
    {
        ApiKeyRow row;
        row.key = query.value(0).toString();
        row.scope = query.value(1).toString();
        row.deletable = query.value(2).toBool();
        rows.append(row);
    }
    return rows;
}

bool SqliteStorage::ensureSchema()
{
    if (!m_isOpen)
    {
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON;"));
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL;"));

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS records ("
            "collection TEXT NOT NULL,"
            "doc TEXT NOT NULL,"
            "ts INTEGER NOT NULL,"
            "data TEXT NOT NULL,"
            "PRIMARY KEY(collection, doc, ts)"
            ");")))
    {
        logError(query, QStringLiteral("Failed to create records table"));
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS key_values ("
            "collection TEXT NOT NULL,"
            "key TEXT NOT NULL,"
            "value TEXT NOT NULL,"
            "PRIMARY KEY(collection, key)"
            ");")))
    {
        logError(query, QStringLiteral("Failed to create key_values table"));
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS api_keys ("
            "api_key TEXT PRIMARY KEY,"
            "scope TEXT NOT NULL,"
            "deletable INTEGER NOT NULL"
            ");")))
    {
        logError(query, QStringLiteral("Failed to create api_keys table"));
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_records_collection_doc_ts ON records(collection, doc, ts);")))
    {
        logError(query, QStringLiteral("Failed to create collection/doc index"));
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_records_collection_ts ON records(collection, ts);")))
    {
        logError(query, QStringLiteral("Failed to create collection/timestamp index"));
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_records_doc_ts ON records(doc, ts);")))
    {
        logError(query, QStringLiteral("Failed to create doc/timestamp index"));
        return false;
    }

    return true;
}

void SqliteStorage::logError(const QSqlQuery &query, const QString &context) const
{
    qWarning() << context << ":" << query.lastError().text();
}

bool SqliteStorage::beginTransaction()
{
    if (!m_isOpen)
    {
        return false;
    }
    if (!m_db.transaction())
    {
        qWarning() << "Failed to begin SQLite transaction:" << m_db.lastError().text();
        return false;
    }
    return true;
}

bool SqliteStorage::commitTransaction()
{
    if (!m_isOpen)
    {
        return false;
    }
    if (!m_db.commit())
    {
        qWarning() << "Failed to commit SQLite transaction:" << m_db.lastError().text();
        return false;
    }
    return true;
}

void SqliteStorage::rollbackTransaction()
{
    if (!m_isOpen)
    {
        return;
    }
    if (!m_db.rollback())
    {
        qWarning() << "Failed to rollback SQLite transaction:" << m_db.lastError().text();
    }
}
