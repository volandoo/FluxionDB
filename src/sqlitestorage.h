#ifndef SQLITESTORAGE_H
#define SQLITESTORAGE_H

#include <cstdint>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <vector>

class SqliteStorage
{
public:
    struct StoredRecord
    {
        std::string document;
        std::int64_t timestamp = 0;
        std::string data;
    };

    struct StoredKeyValue
    {
        std::string key;
        std::string value;
    };

    struct ApiKeyRow
    {
        std::string key;
        std::string scope;
        bool deletable = false;
    };

    SqliteStorage() = default;
    ~SqliteStorage();

    bool initialize(std::string_view dataFolder);
    void shutdown();
    bool isOpen() const;

    std::vector<std::string> collections() const;
    std::vector<StoredRecord> fetchRecords(std::string_view collection) const;
    std::vector<StoredKeyValue> fetchKeyValues(std::string_view collection) const;

    bool upsertRecord(std::string_view collection, std::string_view document, std::int64_t timestamp, std::string_view data);
    bool deleteRecord(std::string_view collection, std::string_view document, std::int64_t timestamp);
    bool deleteRecordsInRange(std::string_view collection, std::string_view document, std::int64_t fromTs, std::int64_t toTs);
    bool deleteDocument(std::string_view collection, std::string_view document);
    bool deleteCollection(std::string_view collection);
    bool upsertKeyValue(std::string_view collection, std::string_view key, std::string_view value);
    bool removeKeyValue(std::string_view collection, std::string_view key);

    bool upsertApiKey(std::string_view key, std::string_view scope, bool deletable);
    bool deleteApiKey(std::string_view key);
    std::vector<ApiKeyRow> fetchApiKeys() const;
    bool beginTransaction();
    bool commitTransaction();
    void rollbackTransaction();
    bool checkpointWal(bool truncate);

private:
    bool ensureSchema();
    bool exec(std::string_view sql) const;
    void logError(std::string_view context) const;
    static void bindText(sqlite3_stmt* stmt, int index, std::string_view value);
    static std::string columnText(sqlite3_stmt* stmt, int index);

    sqlite3* m_db = nullptr;
    std::string m_dbFilePath;
};

#endif // SQLITESTORAGE_H
