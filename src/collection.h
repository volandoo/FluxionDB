#ifndef COLLECTION_H
#define COLLECTION_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "datarecord.h"

class SqliteStorage;

void appendJsonEscapedUtf8(std::string& out, std::string_view data);

class Collection {
public:
    using RecordMap = std::unordered_map<std::string, DataRecord*>;
    using RecordList = std::vector<DataRecord*>;
    using SessionMap = std::unordered_map<std::string, std::vector<DataRecord*>>;

    explicit Collection(std::string name, SqliteStorage* storage);
    ~Collection();

    void insert(std::int64_t timestamp, std::string_view key, std::string_view data);
    DataRecord* getLatestRecordForDocument(std::string_view key, std::int64_t timestamp);
    DataRecord* getEarliestRecordForDocument(std::string_view key, std::int64_t timestamp);
    RecordMap getAllRecords(std::int64_t timestamp,
                            std::string_view key,
                            std::int64_t from = 0,
                            const std::regex* keyRegex = nullptr,
                            std::string_view where = {},
                            std::string_view filter = {},
                            const std::regex* whereRegex = nullptr,
                            const std::regex* filterRegex = nullptr);
    RecordList getAllRecordsForDocument(std::string_view key,
                                        std::int64_t from,
                                        std::int64_t to,
                                        bool reverse = false,
                                        std::int64_t limit = 0,
                                        std::string_view where = {},
                                        std::string_view filter = {},
                                        const std::regex* whereRegex = nullptr,
                                        const std::regex* filterRegex = nullptr);
    SessionMap getSessionData(std::int64_t from, std::int64_t to);
    
    void setValueForKey(std::string_view key, std::string_view value);
    std::string getValueForKey(std::string_view key) const;
    const std::string* getValueRefForKey(std::string_view key) const;
    void removeValueForKey(std::string_view key);
    std::unordered_map<std::string, std::string> getAllValues(const std::regex* keyRegex = nullptr) const;
    void appendAllValuesAsJson(std::string& out, const std::regex* keyRegex = nullptr) const;
    std::vector<std::string> getAllKeys() const;
    void appendAllKeysAsJsonArray(std::string& out) const;

    void clearDocument(std::string_view key);
    void deleteRecord(std::string_view key, std::int64_t ts);
    void deleteRecordsInRange(std::string_view key, std::int64_t fromTs, std::int64_t toTs);
    void flushToDisk();
    void loadFromDisk();
    bool isEmpty() const {
        return m_data.empty();
    }

private:
    class RecordPool {
    public:
        DataRecord* create(std::int64_t timestamp, std::string_view data, bool isNew);
        void release(DataRecord* record);
        void clearFreeList();

    private:
        static constexpr std::size_t BlockSize = 4096;
        std::vector<std::unique_ptr<DataRecord[]>> m_blocks;
        std::vector<DataRecord*> m_free;
        std::size_t m_nextIndex = BlockSize;
    };

    void insertInternal(std::int64_t timestamp, std::string_view key, std::string_view data, bool persistToStorage);
    int getLatestRecordIndex(const std::vector<DataRecord*>& records, std::int64_t timestamp) const;
    int getEarliestRecordIndex(const std::vector<DataRecord*>& records, std::int64_t timestamp) const;
    
    std::string m_name;
    std::unordered_map<std::string, std::vector<DataRecord*>> m_data;
    std::unordered_map<std::string, std::string> m_key_value;
    RecordPool m_recordPool;
    SqliteStorage* m_storage = nullptr;
    bool m_hasNewRecords = false;
    std::mutex m_flushMutex;
};

#endif // COLLECTION_H
