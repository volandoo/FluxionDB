#include "collection.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <algorithm>
#include <iterator>
#include <utility>
#include "sqlitestorage.h"

#ifdef __linux__ 
#include <malloc.h>
#endif

Collection::Collection(const QString &name, SqliteStorage *storage)
{
    m_name = name;
    m_storage = storage;
    m_hasNewRecords = false;
}

Collection::~Collection() {
    flushToDisk();
    m_data.clear();
    m_data.rehash(0);
#ifdef __linux__
    malloc_trim(0);
#endif
    qInfo() << "Collection deleted from memory" << m_name;    
}

void Collection::insert(qint64 timestamp, const QString &key, const QString &data)
{
    const bool shouldPersist = (m_storage != nullptr);
    insertInternal(timestamp, key, data, shouldPersist);
}

void Collection::insertInternal(qint64 timestamp, const QString &key, const QString &data, bool persistToStorage)
{
    // Create new record
    auto record = std::make_unique<DataRecord>();
    record->timestamp = timestamp;
    record->data = data.toStdString();
    record->isNew = persistToStorage;
    const bool shouldMarkNew = record->isNew;

    // Get or create vector for this key
    auto it = m_data.find(key);
    if (it == m_data.end())
    {
        std::vector<std::unique_ptr<DataRecord>> newVector;
        if (shouldMarkNew)
        {
            m_hasNewRecords = true;
        }
        newVector.push_back(std::move(record));
        m_data.emplace(key, std::move(newVector));
        return;
    }

    auto &records = it->second;
    if (records.empty())
    {
        records.push_back(std::move(record));
        return;
    }

    // Find the position to insert using binary search
    auto vecIt = std::lower_bound(records.begin(), records.end(), timestamp,
                                  [](const std::unique_ptr<DataRecord> &a, qint64 b)
                                  {
                                      return a->timestamp < b;
                                  });

    // Check if we found a record with the same timestamp
    if (vecIt != records.end() && (*vecIt)->timestamp == timestamp)
    {
        *vecIt = std::move(record); // Replace existing record
    }
    else
    {
        records.insert(vecIt, std::move(record)); // Insert at the correct position
    }

    if (shouldMarkNew)
    {
        m_hasNewRecords = true;
    }
}

DataRecord *Collection::getLatestRecordForDocument(const QString &key, qint64 timestamp)
{
    auto it = m_data.find(key);
    if (it == m_data.end())
    {
        return nullptr;
    }

    const int index = getLatestRecordIndex(it->second, timestamp);
    if (index == -1)
    {
        return nullptr;
    }

    return it->second[index].get();
}

DataRecord *Collection::getEarliestRecordForDocument(const QString &key, qint64 timestamp)
{
    auto it = m_data.find(key);
    if (it == m_data.end())
    {
        return nullptr;
    }

    const int index = getEarliestRecordIndex(it->second, timestamp);
    if (index == -1)
    {
        return nullptr;
    }

    return it->second[index].get();
}

QHash<QString, DataRecord *> Collection::getAllRecords(qint64 timestamp, const QString &key, qint64 from, const QRegularExpression *keyRegex)
{
    QHash<QString, DataRecord *> result;
    const bool hasRegex = keyRegex != nullptr && keyRegex->isValid();
    if (hasRegex || key.isEmpty())
    {
        for (const auto &[docKey, records] : m_data)
        {
            if (hasRegex && !keyRegex->match(docKey).hasMatch())
            {
                continue;
            }
            if (!key.isEmpty() && docKey != key)
            {
                continue;
            }
            const int index = getLatestRecordIndex(records, timestamp);
            if (index != -1)
            {
                auto record = records[index].get();
                if (from == 0 || record->timestamp >= from) {
                    result.insert(docKey, record);
                }
            }
        }
    }
    else
    {
        auto it = m_data.find(key);
        if (it == m_data.end())
        {
            return result;
        }
        const int index = getLatestRecordIndex(it->second, timestamp);
        if (index != -1)
        {
            auto record = it->second[index].get();
            if (from == 0 || record->timestamp >= from) {
                result.insert(key, record);
            }
        }
    }
    return result;
}

QHash<QString, QList<DataRecord *>> Collection::getSessionData(qint64 from, qint64 to)
{
    QHash<QString, QList<DataRecord *>> result;
    for (const auto &[key, records] : m_data)
    {
        if (from > to)
        {
            continue;
        }
        const int startIndex = getEarliestRecordIndex(records, from);
        if (startIndex == -1)
        {
            continue;
        }
        const int endIndex = getLatestRecordIndex(records, to);
            if (endIndex == -1)
        {
            continue;
        }
        for (int i = startIndex; i <= endIndex; ++i)
        {
            result[key].append(records[i].get());
        }
    }
    return result;
}

QList<DataRecord *> Collection::getAllRecordsForDocument(const QString &key, qint64 from, qint64 to, bool reverse, qint64 limit)
{
    QList<DataRecord *> result;
    auto it = m_data.find(key);
    if (it == m_data.end())
    {
        return result;
    }

    if (from > to)
    {
        return result;
    }

    const int startIndex = getEarliestRecordIndex(it->second, from);
    if (startIndex == -1)
    {
        return result;
    }

    const int endIndex = getLatestRecordIndex(it->second, to);
    if (endIndex == -1)
    {
        return result;
    }

    for (int i = startIndex; i <= endIndex; ++i)
    {
        result.append(it->second[i].get());
    }

    if (reverse)
    {
        std::reverse(result.begin(), result.end());
    }

    if (limit > 0 && result.size() > limit)
    {
        result.resize(limit);
    }

    return result;
}

void Collection::clearDocument(const QString &key)
{
    auto it = m_data.find(key);
    if (it != m_data.end())
    {
        std::vector<std::unique_ptr<DataRecord>>{}.swap(it->second);
        m_data.erase(it);
        m_data.rehash(0);
#ifdef __linux__
        malloc_trim(0);
#endif
        qInfo() << "Document deleted from memory" << m_name << ":" << key;
    }

    if (m_storage != nullptr)
    {
        m_storage->deleteDocument(m_name, key);
    }
}

void Collection::deleteRecord(const QString &key, qint64 ts)
{
    auto it = m_data.find(key);
    if (it == m_data.end()) {
        return;
    }
    auto &records = it->second;
    auto vecIt = std::lower_bound(records.begin(), records.end(), ts,
                                  [](const std::unique_ptr<DataRecord> &a, qint64 b)
                                  {
                                      return a->timestamp < b;
                                  });   
    if (vecIt == records.end() || (*vecIt)->timestamp != ts) {
        return;
    }
    records.erase(vecIt);
    if (records.empty()) {
        std::vector<std::unique_ptr<DataRecord>>{}.swap(records);
        m_data.erase(it);
        m_data.rehash(0);
#ifdef __linux__
        malloc_trim(0);
#endif
        if (m_storage != nullptr)
        {
            m_storage->deleteRecord(m_name, key, ts);
        }
        return;
    }    

    const auto capacity = records.capacity();
    const auto size = records.size();
    if (capacity > 0 && size * 2 < capacity)
    {
        std::vector<std::unique_ptr<DataRecord>> compacted;
        compacted.reserve(size);
        std::move(records.begin(), records.end(), std::back_inserter(compacted));
        records.swap(compacted);
#ifdef __linux__
        malloc_trim(0);
#endif
    }
    if (m_storage != nullptr)
    {
        m_storage->deleteRecord(m_name, key, ts);
    }
}

void Collection::deleteRecordsInRange(const QString &key, qint64 fromTs, qint64 toTs)
{
    auto it = m_data.find(key);
    if (it == m_data.end()) {
        return;
    }
    auto &records = it->second;
    
    // Find the first record >= fromTs
    auto beginIt = std::lower_bound(records.begin(), records.end(), fromTs,
                                    [](const std::unique_ptr<DataRecord> &a, qint64 b)
                                    {
                                        return a->timestamp < b;
                                    });
    
    // Find the first record > toTs
    auto endIt = std::upper_bound(records.begin(), records.end(), toTs,
                                  [](qint64 b, const std::unique_ptr<DataRecord> &a)
                                  {
                                      return b < a->timestamp;
                                  });
    
    if (beginIt == records.end() || beginIt >= endIt) {
        return;
    }
    
    records.erase(beginIt, endIt);
    if (records.empty()) {
        std::vector<std::unique_ptr<DataRecord>>{}.swap(records);
        m_data.erase(it);
        m_data.rehash(0);
#ifdef __linux__
        malloc_trim(0);
#endif
        if (m_storage != nullptr)
        {
            m_storage->deleteRecordsInRange(m_name, key, fromTs, toTs);
        }
        return;
    }
    
    const auto capacity = records.capacity();
    const auto size = records.size();
    if (capacity > 0 && size * 2 < capacity)
    {
        std::vector<std::unique_ptr<DataRecord>> compacted;
        compacted.reserve(size);
        std::move(records.begin(), records.end(), std::back_inserter(compacted));
        records.swap(compacted);
#ifdef __linux__
        malloc_trim(0);
#endif
    }
    if (m_storage != nullptr)
    {
        m_storage->deleteRecordsInRange(m_name, key, fromTs, toTs);
    }
}

int Collection::getLatestRecordIndex(const std::vector<std::unique_ptr<DataRecord>> &records, qint64 timestamp)
{
    if (records.empty())
    {
        return -1;
    }

    auto vecIt = std::upper_bound(records.begin(), records.end(), timestamp,
                                  [](qint64 b, const std::unique_ptr<DataRecord> &a)
                                  {
                                      return b < a->timestamp;
                                  });

    if (vecIt == records.begin())
    {
        if (records.front()->timestamp > timestamp)
        {
            return -1;
        }
    }
    else
    {
        --vecIt;
    }

    return vecIt - records.begin();
}

int Collection::getEarliestRecordIndex(const std::vector<std::unique_ptr<DataRecord>> &records, qint64 timestamp)
{
    if (records.empty())
    {
        return -1;
    }

    auto vecIt = std::lower_bound(records.begin(), records.end(), timestamp,
                                  [](const std::unique_ptr<DataRecord> &a, qint64 b)
                                  {
                                      return a->timestamp < b;
                                  });

    if (vecIt == records.end())
    {
        return -1;
    }

    return vecIt - records.begin();
}

// key value methods

void Collection::setValueForKey(const QString &key, const QString &value)
{
    m_key_vaue[key] = value.toStdString();
    if (m_storage != nullptr)
    {
        m_storage->upsertKeyValue(m_name, key, value);
    }
}

QString Collection::getValueForKey(const QString &key)
{
    auto it = m_key_vaue.find(key);
    if (it == m_key_vaue.end())
    {
        return "";
    }
    return QString::fromStdString(it->second);
}

void Collection::removeValueForKey(const QString &key)
{
    m_key_vaue.erase(key);
    m_key_vaue.rehash(0);
#ifdef __linux__
    malloc_trim(0);
#endif
    if (m_storage != nullptr)
    {
        m_storage->removeKeyValue(m_name, key);
    }
}

QHash<QString, QString> Collection::getAllValues(const QRegularExpression *keyRegex)
{
    QHash<QString, QString> result;
    const bool hasRegex = keyRegex != nullptr && keyRegex->isValid();
    for (auto &[key, value] : m_key_vaue)
    {
        if (hasRegex && !keyRegex->match(key).hasMatch())
        {
            continue;
        }
        result.insert(key, QString::fromStdString(value));
    }
    return result;
}

QList<QString> Collection::getAllKeys()
{
    QList<QString> result;
    for (auto &[key, value] : m_key_vaue)
    {
        result.append(key);
    }
    return result;
}

void Collection::flushToDisk()
{
    QElapsedTimer timer;
    timer.start();

    QMutexLocker locker(&m_flushMutex);

    if (m_storage == nullptr) {
        return;
    }

    if (!m_hasNewRecords)
    {
        return;
    }

    const bool startedTransaction = m_storage->beginTransaction();
    if (!startedTransaction)
    {
        qWarning() << "Failed to start transaction for flushing collection" << m_name;
        return;
    }
    bool keepFlushing = false;
    int count = 0;
    for (auto &[doc, records] : m_data)
    {
        for (auto &record : records)
        {
            if (!record->isNew)
            {
                continue;
            }
            if (m_storage->upsertRecord(m_name, doc, record->timestamp, QString::fromStdString(record->data)))
            {
                record->isNew = false;
                count++;
            }
            else
            {
                qWarning() << "Failed to upsert record for collection" << m_name << "doc" << doc << "timestamp" << record->timestamp;
                keepFlushing = true;
            }
        }
    }
    if (!keepFlushing)
    {
        m_hasNewRecords = false;
    }
    else
    {
        m_hasNewRecords = false;
        for (const auto &[doc, records] : m_data)
        {
            for (const auto &record : records)
            {
                if (record->isNew)
                {
                    m_hasNewRecords = true;
                    break;
                }
            }
            if (m_hasNewRecords)
            {
                break;
            }
        }
    }

    if (startedTransaction)
    {
        if (!m_storage->commitTransaction())
        {
            m_storage->rollbackTransaction();
        }
    }
    const qint64 elapsedNs = timer.nsecsElapsed();
    qInfo() << "Flushed" << count << "new records to SQLite for collection" << m_name << "in" << elapsedNs << "ns";
}

void Collection::loadFromDisk()
{
    if (m_storage == nullptr)
    {
        return;
    }

    qDebug() << "Loading collection from SQLite" << m_name;
    m_data.clear();
    m_data.rehash(0);
    m_key_vaue.clear();
    m_key_vaue.rehash(0);
    m_hasNewRecords = false;

    const auto records = m_storage->fetchRecords(m_name);
    for (const auto &record : records)
    {
        insertInternal(record.timestamp, record.document, record.data, false);
    }

    const auto keyValues = m_storage->fetchKeyValues(m_name);
    for (const auto &kv : keyValues)
    {
        m_key_vaue[kv.key] = kv.value.toStdString();
    }
    qDebug() << "Done loading collection from SQLite" << m_name;
}
