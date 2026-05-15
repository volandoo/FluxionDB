#include "sqlitestorage.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <unordered_set>

namespace {

class Statement {
public:
    Statement(sqlite3* db, std::string_view sql) : m_db(db)
    {
        if (sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &m_stmt, nullptr) != SQLITE_OK)
        {
            m_stmt = nullptr;
        }
    }

    ~Statement()
    {
        if (m_stmt != nullptr)
        {
            sqlite3_finalize(m_stmt);
        }
    }

    sqlite3_stmt* get() const { return m_stmt; }
    explicit operator bool() const { return m_stmt != nullptr; }

private:
    sqlite3* m_db = nullptr;
    sqlite3_stmt* m_stmt = nullptr;
};

bool stepDone(sqlite3_stmt* stmt)
{
    return sqlite3_step(stmt) == SQLITE_DONE;
}

} // namespace

SqliteStorage::~SqliteStorage()
{
    shutdown();
}

bool SqliteStorage::initialize(std::string_view dataFolder)
{
    if (dataFolder.empty())
    {
        std::cerr << "SQLite storage disabled because data folder is empty\n";
        return false;
    }

    if (m_db != nullptr)
    {
        return true;
    }

    std::filesystem::path dir{std::string(dataFolder)};
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec)
    {
        std::cerr << "Failed to create data directory for SQLite storage: " << dir << ": " << ec.message() << '\n';
        return false;
    }

    m_dbFilePath = (dir / "fluxion.db").string();
    if (sqlite3_open(m_dbFilePath.c_str(), &m_db) != SQLITE_OK)
    {
        logError("Failed to open SQLite database");
        shutdown();
        return false;
    }

    if (!ensureSchema())
    {
        shutdown();
        return false;
    }

    std::cerr << "SQLite storage initialized at " << m_dbFilePath << '\n';
    return true;
}

void SqliteStorage::shutdown()
{
    if (m_db == nullptr)
    {
        return;
    }

    checkpointWal(true);
    sqlite3_close(m_db);
    m_db = nullptr;
}

bool SqliteStorage::isOpen() const
{
    return m_db != nullptr;
}

std::vector<std::string> SqliteStorage::collections() const
{
    std::vector<std::string> result;
    if (m_db == nullptr)
    {
        return result;
    }

    std::unordered_set<std::string> names;
    for (std::string_view sql : {
             std::string_view("SELECT DISTINCT collection FROM records"),
             std::string_view("SELECT DISTINCT collection FROM key_values")})
    {
        Statement stmt(m_db, sql);
        if (!stmt)
        {
            logError("Failed to fetch collection names");
            continue;
        }
        while (sqlite3_step(stmt.get()) == SQLITE_ROW)
        {
            names.insert(columnText(stmt.get(), 0));
        }
    }

    result.assign(names.begin(), names.end());
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<SqliteStorage::StoredRecord> SqliteStorage::fetchRecords(std::string_view collection) const
{
    std::vector<StoredRecord> rows;
    if (m_db == nullptr)
    {
        return rows;
    }

    Statement stmt(m_db, "SELECT doc, ts, data FROM records WHERE collection = ? ORDER BY ts ASC");
    if (!stmt)
    {
        logError("Failed to prepare fetch records");
        return rows;
    }
    bindText(stmt.get(), 1, collection);

    while (sqlite3_step(stmt.get()) == SQLITE_ROW)
    {
        StoredRecord record;
        record.document = columnText(stmt.get(), 0);
        record.timestamp = sqlite3_column_int64(stmt.get(), 1);
        record.data = columnText(stmt.get(), 2);
        rows.push_back(std::move(record));
    }
    return rows;
}

std::vector<SqliteStorage::StoredKeyValue> SqliteStorage::fetchKeyValues(std::string_view collection) const
{
    std::vector<StoredKeyValue> rows;
    if (m_db == nullptr)
    {
        return rows;
    }

    Statement stmt(m_db, "SELECT key, value FROM key_values WHERE collection = ?");
    if (!stmt)
    {
        logError("Failed to prepare fetch key values");
        return rows;
    }
    bindText(stmt.get(), 1, collection);

    while (sqlite3_step(stmt.get()) == SQLITE_ROW)
    {
        rows.push_back({columnText(stmt.get(), 0), columnText(stmt.get(), 1)});
    }
    return rows;
}

bool SqliteStorage::upsertRecord(std::string_view collection, std::string_view document, std::int64_t timestamp, std::string_view data)
{
    if (m_db == nullptr)
    {
        return false;
    }
    Statement stmt(m_db, "INSERT OR REPLACE INTO records (ts, collection, doc, data) VALUES (?, ?, ?, ?)");
    if (!stmt)
    {
        logError("Failed to prepare upsert record");
        return false;
    }
    sqlite3_bind_int64(stmt.get(), 1, timestamp);
    bindText(stmt.get(), 2, collection);
    bindText(stmt.get(), 3, document);
    bindText(stmt.get(), 4, data);
    if (!stepDone(stmt.get()))
    {
        logError("Failed to upsert record");
        return false;
    }
    return true;
}

bool SqliteStorage::deleteRecord(std::string_view collection, std::string_view document, std::int64_t timestamp)
{
    if (m_db == nullptr)
    {
        return false;
    }
    Statement stmt(m_db, "DELETE FROM records WHERE ts = ? AND collection = ? AND doc = ?");
    if (!stmt)
    {
        logError("Failed to prepare delete record");
        return false;
    }
    sqlite3_bind_int64(stmt.get(), 1, timestamp);
    bindText(stmt.get(), 2, collection);
    bindText(stmt.get(), 3, document);
    return stepDone(stmt.get());
}

bool SqliteStorage::deleteRecordsInRange(std::string_view collection, std::string_view document, std::int64_t fromTs, std::int64_t toTs)
{
    if (m_db == nullptr)
    {
        return false;
    }
    Statement stmt(m_db, "DELETE FROM records WHERE collection = ? AND doc = ? AND ts >= ? AND ts <= ?");
    if (!stmt)
    {
        logError("Failed to prepare delete records in range");
        return false;
    }
    bindText(stmt.get(), 1, collection);
    bindText(stmt.get(), 2, document);
    sqlite3_bind_int64(stmt.get(), 3, fromTs);
    sqlite3_bind_int64(stmt.get(), 4, toTs);
    return stepDone(stmt.get());
}

bool SqliteStorage::deleteDocument(std::string_view collection, std::string_view document)
{
    if (m_db == nullptr)
    {
        return false;
    }
    Statement stmt(m_db, "DELETE FROM records WHERE collection = ? AND doc = ?");
    if (!stmt)
    {
        logError("Failed to prepare delete document");
        return false;
    }
    bindText(stmt.get(), 1, collection);
    bindText(stmt.get(), 2, document);
    return stepDone(stmt.get());
}

bool SqliteStorage::deleteCollection(std::string_view collection)
{
    if (m_db == nullptr)
    {
        return false;
    }
    Statement records(m_db, "DELETE FROM records WHERE collection = ?");
    if (!records)
    {
        logError("Failed to prepare delete collection records");
        return false;
    }
    bindText(records.get(), 1, collection);
    if (!stepDone(records.get()))
    {
        logError("Failed to delete collection records");
        return false;
    }

    Statement kv(m_db, "DELETE FROM key_values WHERE collection = ?");
    if (!kv)
    {
        logError("Failed to prepare delete collection key values");
        return false;
    }
    bindText(kv.get(), 1, collection);
    if (!stepDone(kv.get()))
    {
        logError("Failed to delete collection key values");
        return false;
    }
    return true;
}

bool SqliteStorage::upsertKeyValue(std::string_view collection, std::string_view key, std::string_view value)
{
    if (m_db == nullptr)
    {
        return false;
    }
    Statement stmt(m_db, "INSERT OR REPLACE INTO key_values (collection, key, value) VALUES (?, ?, ?)");
    if (!stmt)
    {
        logError("Failed to prepare upsert key value");
        return false;
    }
    bindText(stmt.get(), 1, collection);
    bindText(stmt.get(), 2, key);
    bindText(stmt.get(), 3, value);
    return stepDone(stmt.get());
}

bool SqliteStorage::removeKeyValue(std::string_view collection, std::string_view key)
{
    if (m_db == nullptr)
    {
        return false;
    }
    Statement stmt(m_db, "DELETE FROM key_values WHERE collection = ? AND key = ?");
    if (!stmt)
    {
        logError("Failed to prepare remove key value");
        return false;
    }
    bindText(stmt.get(), 1, collection);
    bindText(stmt.get(), 2, key);
    return stepDone(stmt.get());
}

bool SqliteStorage::upsertApiKey(std::string_view key, std::string_view scope, bool deletable)
{
    if (m_db == nullptr)
    {
        return false;
    }
    Statement stmt(m_db, "INSERT OR REPLACE INTO api_keys (api_key, scope, deletable) VALUES (?, ?, ?)");
    if (!stmt)
    {
        logError("Failed to prepare upsert API key");
        return false;
    }
    bindText(stmt.get(), 1, key);
    bindText(stmt.get(), 2, scope);
    sqlite3_bind_int(stmt.get(), 3, deletable ? 1 : 0);
    return stepDone(stmt.get());
}

bool SqliteStorage::deleteApiKey(std::string_view key)
{
    if (m_db == nullptr)
    {
        return false;
    }
    Statement stmt(m_db, "DELETE FROM api_keys WHERE api_key = ?");
    if (!stmt)
    {
        logError("Failed to prepare delete API key");
        return false;
    }
    bindText(stmt.get(), 1, key);
    return stepDone(stmt.get());
}

std::vector<SqliteStorage::ApiKeyRow> SqliteStorage::fetchApiKeys() const
{
    std::vector<ApiKeyRow> rows;
    if (m_db == nullptr)
    {
        return rows;
    }

    Statement stmt(m_db, "SELECT api_key, scope, deletable FROM api_keys");
    if (!stmt)
    {
        logError("Failed to fetch API keys");
        return rows;
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW)
    {
        rows.push_back({
            columnText(stmt.get(), 0),
            columnText(stmt.get(), 1),
            sqlite3_column_int(stmt.get(), 2) != 0,
        });
    }
    return rows;
}

bool SqliteStorage::ensureSchema()
{
    if (m_db == nullptr)
    {
        return false;
    }

    return exec("PRAGMA foreign_keys = ON;") &&
           exec("PRAGMA journal_mode = WAL;") &&
           exec("PRAGMA synchronous = NORMAL;") &&
           exec("PRAGMA wal_autocheckpoint = 4096;") &&
           exec("PRAGMA journal_size_limit = 67108864;") &&
           exec("CREATE TABLE IF NOT EXISTS records ("
                "collection TEXT NOT NULL,"
                "doc TEXT NOT NULL,"
                "ts INTEGER NOT NULL,"
                "data TEXT NOT NULL,"
                "PRIMARY KEY(collection, doc, ts)"
                ");") &&
           exec("CREATE TABLE IF NOT EXISTS key_values ("
                "collection TEXT NOT NULL,"
                "key TEXT NOT NULL,"
                "value TEXT NOT NULL,"
                "PRIMARY KEY(collection, key)"
                ");") &&
           exec("CREATE TABLE IF NOT EXISTS api_keys ("
                "api_key TEXT PRIMARY KEY,"
                "scope TEXT NOT NULL,"
                "deletable INTEGER NOT NULL"
                ");") &&
           exec("CREATE INDEX IF NOT EXISTS idx_records_collection_doc_ts ON records(collection, doc, ts);") &&
           exec("CREATE INDEX IF NOT EXISTS idx_records_collection_ts ON records(collection, ts);") &&
           exec("CREATE INDEX IF NOT EXISTS idx_records_doc_ts ON records(doc, ts);");
}

bool SqliteStorage::beginTransaction()
{
    return exec("BEGIN IMMEDIATE TRANSACTION;");
}

bool SqliteStorage::commitTransaction()
{
    return exec("COMMIT;");
}

void SqliteStorage::rollbackTransaction()
{
    exec("ROLLBACK;");
}

bool SqliteStorage::checkpointWal(bool truncate)
{
    if (m_db == nullptr)
    {
        return false;
    }

    const std::string sql = truncate ? "PRAGMA wal_checkpoint(TRUNCATE);" : "PRAGMA wal_checkpoint(PASSIVE);";
    Statement stmt(m_db, sql);
    if (!stmt)
    {
        logError("Failed to prepare SQLite WAL checkpoint");
        return false;
    }
    if (sqlite3_step(stmt.get()) != SQLITE_ROW)
    {
        logError("Failed to checkpoint SQLite WAL");
        return false;
    }
    const int busy = sqlite3_column_int(stmt.get(), 0);
    if (busy != 0)
    {
        std::cerr << "SQLite WAL checkpoint could not complete because the database is busy\n";
        return false;
    }
    return true;
}

bool SqliteStorage::exec(std::string_view sql) const
{
    if (m_db == nullptr)
    {
        return false;
    }
    char* error = nullptr;
    const int rc = sqlite3_exec(m_db, std::string(sql).c_str(), nullptr, nullptr, &error);
    if (rc != SQLITE_OK)
    {
        std::cerr << "SQLite error: " << (error != nullptr ? error : sqlite3_errmsg(m_db)) << '\n';
        sqlite3_free(error);
        return false;
    }
    return true;
}

void SqliteStorage::logError(std::string_view context) const
{
    std::cerr << context << ": " << (m_db != nullptr ? sqlite3_errmsg(m_db) : "database is closed") << '\n';
}

void SqliteStorage::bindText(sqlite3_stmt* stmt, int index, std::string_view value)
{
    sqlite3_bind_text(stmt, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

std::string SqliteStorage::columnText(sqlite3_stmt* stmt, int index)
{
    const unsigned char* text = sqlite3_column_text(stmt, index);
    const int bytes = sqlite3_column_bytes(stmt, index);
    if (text == nullptr || bytes <= 0)
    {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(bytes));
}
