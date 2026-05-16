#include "collection.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <utility>

#include "sqlitestorage.h"

void appendJsonEscapedUtf8(std::string& out, std::string_view data)
{
    for (unsigned char c : data)
    {
        switch (c)
        {
        case '"':  out.append("\\\""); break;
        case '\\': out.append("\\\\"); break;
        case '\b': out.append("\\b"); break;
        case '\f': out.append("\\f"); break;
        case '\n': out.append("\\n"); break;
        case '\r': out.append("\\r"); break;
        case '\t': out.append("\\t"); break;
        default:
            if (c < 0x20)
            {
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out.append(buf, 6);
            }
            else
            {
                out.push_back(static_cast<char>(c));
            }
        }
    }
}

namespace {

bool regexMatches(const std::regex* regex, std::string_view text)
{
    return regex != nullptr && std::regex_search(text.begin(), text.end(), *regex);
}

bool recordPassesPredicates(const DataRecord* record,
                            std::string_view whereText,
                            std::string_view filterText,
                            const std::regex* whereRegex,
                            const std::regex* filterRegex)
{
    const bool hasWhere = whereRegex != nullptr || !whereText.empty();
    const bool hasFilter = filterRegex != nullptr || !filterText.empty();
    if (!hasWhere && !hasFilter)
    {
        return true;
    }

    if (whereRegex != nullptr && !regexMatches(whereRegex, record->data))
    {
        return false;
    }
    if (whereRegex == nullptr && hasWhere && record->data.find(whereText) == std::string::npos)
    {
        return false;
    }
    if (filterRegex != nullptr && regexMatches(filterRegex, record->data))
    {
        return false;
    }
    if (filterRegex == nullptr && hasFilter && record->data.find(filterText) != std::string::npos)
    {
        return false;
    }

    return true;
}

} // namespace

DataRecord* Collection::RecordPool::create(std::int64_t timestamp, std::string_view data, bool isNew)
{
    DataRecord* record = nullptr;
    if (!m_free.empty())
    {
        record = m_free.back();
        m_free.pop_back();
    }
    else
    {
        if (m_nextIndex >= BlockSize)
        {
            m_blocks.emplace_back(std::make_unique<DataRecord[]>(BlockSize));
            m_nextIndex = 0;
        }
        record = &m_blocks.back()[m_nextIndex++];
    }

    record->timestamp = timestamp;
    record->data.assign(data.data(), data.size());
    record->isNew = isNew;
    return record;
}

void Collection::RecordPool::release(DataRecord* record)
{
    if (record == nullptr)
    {
        return;
    }
    record->timestamp = 0;
    record->data.clear();
    record->isNew = false;
    m_free.push_back(record);
}

void Collection::RecordPool::clearFreeList()
{
    m_free.clear();
}

Collection::Collection(std::string name, SqliteStorage* storage)
    : m_name(std::move(name)), m_storage(storage)
{
}

Collection::~Collection()
{
    flushToDisk();
    std::cerr << "Collection deleted from memory " << m_name << '\n';
}

void Collection::insert(std::int64_t timestamp, std::string_view key, std::string_view data)
{
    insertInternal(timestamp, key, data, m_storage != nullptr);
}

void Collection::insertInternal(std::int64_t timestamp, std::string_view key, std::string_view data, bool persistToStorage)
{
    DataRecord* record = m_recordPool.create(timestamp, data, persistToStorage);
    const bool shouldMarkNew = record->isNew;
    std::string keyString(key);

    auto it = m_data.find(keyString);
    if (it == m_data.end())
    {
        std::vector<DataRecord*> newVector;
        newVector.push_back(record);
        m_data.emplace(std::move(keyString), std::move(newVector));
        if (shouldMarkNew)
        {
            m_hasNewRecords = true;
        }
        return;
    }

    auto& records = it->second;
    auto vecIt = std::lower_bound(records.begin(), records.end(), timestamp,
                                  [](const DataRecord* a, std::int64_t b)
                                  {
                                      return a->timestamp < b;
                                  });

    if (vecIt != records.end() && (*vecIt)->timestamp == timestamp)
    {
        m_recordPool.release(*vecIt);
        *vecIt = record;
    }
    else
    {
        records.insert(vecIt, record);
    }

    if (shouldMarkNew)
    {
        m_hasNewRecords = true;
    }
}

DataRecord* Collection::getLatestRecordForDocument(std::string_view key, std::int64_t timestamp)
{
    auto it = m_data.find(std::string(key));
    if (it == m_data.end())
    {
        return nullptr;
    }

    const int index = getLatestRecordIndex(it->second, timestamp);
    return index == -1 ? nullptr : it->second[index];
}

DataRecord* Collection::getEarliestRecordForDocument(std::string_view key, std::int64_t timestamp)
{
    auto it = m_data.find(std::string(key));
    if (it == m_data.end())
    {
        return nullptr;
    }

    const int index = getEarliestRecordIndex(it->second, timestamp);
    return index == -1 ? nullptr : it->second[index];
}

Collection::RecordMap Collection::getAllRecords(std::int64_t timestamp,
                                                std::string_view key,
                                                std::int64_t from,
                                                const std::regex* keyRegex,
                                                std::string_view where,
                                                std::string_view filter,
                                                const std::regex* whereRegex,
                                                const std::regex* filterRegex)
{
    RecordMap result;
    if (keyRegex != nullptr || key.empty())
    {
        for (const auto& [docKey, records] : m_data)
        {
            if (keyRegex != nullptr && !regexMatches(keyRegex, docKey))
            {
                continue;
            }
            const int index = getLatestRecordIndex(records, timestamp);
            if (index != -1)
            {
                DataRecord* record = records[index];
                if ((from == 0 || record->timestamp >= from) &&
                    recordPassesPredicates(record, where, filter, whereRegex, filterRegex))
                {
                    result.emplace(docKey, record);
                }
            }
        }
    }
    else
    {
        auto it = m_data.find(std::string(key));
        if (it == m_data.end())
        {
            return result;
        }
        const int index = getLatestRecordIndex(it->second, timestamp);
        if (index != -1)
        {
            DataRecord* record = it->second[index];
            if ((from == 0 || record->timestamp >= from) &&
                recordPassesPredicates(record, where, filter, whereRegex, filterRegex))
            {
                result.emplace(std::string(key), record);
            }
        }
    }
    return result;
}

Collection::SessionMap Collection::getSessionData(std::int64_t from, std::int64_t to)
{
    SessionMap result;
    for (const auto& [key, records] : m_data)
    {
        if (from > to)
        {
            continue;
        }
        const int startIndex = getEarliestRecordIndex(records, from);
        const int endIndex = getLatestRecordIndex(records, to);
        if (startIndex == -1 || endIndex == -1)
        {
            continue;
        }
        auto& target = result[key];
        target.reserve(static_cast<std::size_t>(endIndex - startIndex + 1));
        for (int i = startIndex; i <= endIndex; ++i)
        {
            target.push_back(records[i]);
        }
    }
    return result;
}

Collection::RecordList Collection::getAllRecordsForDocument(std::string_view key,
                                                            std::int64_t from,
                                                            std::int64_t to,
                                                            bool reverse,
                                                            std::int64_t limit,
                                                            std::string_view where,
                                                            std::string_view filter,
                                                            const std::regex* whereRegex,
                                                            const std::regex* filterRegex)
{
    RecordList result;
    auto it = m_data.find(std::string(key));
    if (it == m_data.end() || from > to)
    {
        return result;
    }

    const int startIndex = getEarliestRecordIndex(it->second, from);
    const int endIndex = getLatestRecordIndex(it->second, to);
    if (startIndex == -1 || endIndex == -1)
    {
        return result;
    }

    const auto appendIfMatches = [&](DataRecord* record) {
        if (recordPassesPredicates(record, where, filter, whereRegex, filterRegex))
        {
            result.push_back(record);
        }
    };

    if (reverse)
    {
        for (int i = endIndex; i >= startIndex; --i)
        {
            appendIfMatches(it->second[i]);
            if (limit > 0 && static_cast<std::int64_t>(result.size()) >= limit)
            {
                break;
            }
        }
        return result;
    }

    for (int i = startIndex; i <= endIndex; ++i)
    {
        appendIfMatches(it->second[i]);
        if (limit > 0 && static_cast<std::int64_t>(result.size()) >= limit)
        {
            break;
        }
    }

    return result;
}

void Collection::clearDocument(std::string_view key)
{
    auto it = m_data.find(std::string(key));
    if (it != m_data.end())
    {
        for (DataRecord* record : it->second)
        {
            m_recordPool.release(record);
        }
        m_data.erase(it);
        std::cerr << "Document deleted from memory " << m_name << ':' << key << '\n';
    }

    if (m_storage != nullptr)
    {
        m_storage->deleteDocument(m_name, key);
    }
}

void Collection::deleteRecord(std::string_view key, std::int64_t ts)
{
    auto it = m_data.find(std::string(key));
    if (it == m_data.end())
    {
        return;
    }
    auto& records = it->second;
    auto vecIt = std::lower_bound(records.begin(), records.end(), ts,
                                  [](const DataRecord* a, std::int64_t b)
                                  {
                                      return a->timestamp < b;
                                  });
    if (vecIt == records.end() || (*vecIt)->timestamp != ts)
    {
        return;
    }

    m_recordPool.release(*vecIt);
    records.erase(vecIt);
    if (records.empty())
    {
        m_data.erase(it);
    }
    else if (records.size() * 3 < records.capacity())
    {
        std::vector<DataRecord*> compacted;
        compacted.reserve(records.size());
        std::move(records.begin(), records.end(), std::back_inserter(compacted));
        records.swap(compacted);
    }

    if (m_storage != nullptr)
    {
        m_storage->deleteRecord(m_name, key, ts);
    }
}

void Collection::deleteRecordsInRange(std::string_view key, std::int64_t fromTs, std::int64_t toTs)
{
    auto it = m_data.find(std::string(key));
    if (it == m_data.end())
    {
        return;
    }
    auto& records = it->second;
    
    auto beginIt = std::lower_bound(records.begin(), records.end(), fromTs,
                                    [](const DataRecord* a, std::int64_t b)
                                    {
                                        return a->timestamp < b;
                                    });
    auto endIt = std::upper_bound(records.begin(), records.end(), toTs,
                                  [](std::int64_t b, const DataRecord* a)
                                  {
                                      return b < a->timestamp;
                                  });
    
    if (beginIt == records.end() || beginIt >= endIt)
    {
        return;
    }

    for (auto iter = beginIt; iter != endIt; ++iter)
    {
        m_recordPool.release(*iter);
    }
    records.erase(beginIt, endIt);
    if (records.empty())
    {
        m_data.erase(it);
    }
    else if (records.size() * 3 < records.capacity())
    {
        std::vector<DataRecord*> compacted;
        compacted.reserve(records.size());
        std::move(records.begin(), records.end(), std::back_inserter(compacted));
        records.swap(compacted);
    }

    if (m_storage != nullptr)
    {
        m_storage->deleteRecordsInRange(m_name, key, fromTs, toTs);
    }
}

int Collection::getLatestRecordIndex(const std::vector<DataRecord*>& records, std::int64_t timestamp) const
{
    if (records.empty())
    {
        return -1;
    }

    auto vecIt = std::upper_bound(records.begin(), records.end(), timestamp,
                                  [](std::int64_t b, const DataRecord* a)
                                  {
                                      return b < a->timestamp;
                                  });

    if (vecIt == records.begin())
    {
        return records.front()->timestamp > timestamp ? -1 : 0;
    }

    --vecIt;
    return static_cast<int>(vecIt - records.begin());
}

int Collection::getEarliestRecordIndex(const std::vector<DataRecord*>& records, std::int64_t timestamp) const
{
    if (records.empty())
    {
        return -1;
    }

    auto vecIt = std::lower_bound(records.begin(), records.end(), timestamp,
                                  [](const DataRecord* a, std::int64_t b)
                                  {
                                      return a->timestamp < b;
                                  });

    return vecIt == records.end() ? -1 : static_cast<int>(vecIt - records.begin());
}

void Collection::setValueForKey(std::string_view key, std::string_view value)
{
    m_key_value[std::string(key)] = std::string(value);
    if (m_storage != nullptr)
    {
        m_storage->upsertKeyValue(m_name, key, value);
    }
}

std::string Collection::getValueForKey(std::string_view key) const
{
    auto it = m_key_value.find(std::string(key));
    return it == m_key_value.end() ? std::string() : it->second;
}

const std::string* Collection::getValueRefForKey(std::string_view key) const
{
    auto it = m_key_value.find(std::string(key));
    return it == m_key_value.end() ? nullptr : &it->second;
}

void Collection::removeValueForKey(std::string_view key)
{
    m_key_value.erase(std::string(key));
    if (m_storage != nullptr)
    {
        m_storage->removeKeyValue(m_name, key);
    }
}

std::unordered_map<std::string, std::string> Collection::getAllValues(const std::regex* keyRegex) const
{
    std::unordered_map<std::string, std::string> result;
    for (const auto& [key, value] : m_key_value)
    {
        if (keyRegex != nullptr && !regexMatches(keyRegex, key))
        {
            continue;
        }
        result.emplace(key, value);
    }
    return result;
}

void Collection::appendAllValuesAsJson(std::string& out, const std::regex* keyRegex) const
{
    out.push_back('{');
    bool first = true;
    for (const auto& [key, value] : m_key_value)
    {
        if (keyRegex != nullptr && !regexMatches(keyRegex, key))
        {
            continue;
        }
        if (!first)
        {
            out.push_back(',');
        }
        first = false;
        out.push_back('"');
        appendJsonEscapedUtf8(out, key);
        out.append("\":\"");
        appendJsonEscapedUtf8(out, value);
        out.push_back('"');
    }
    out.push_back('}');
}

std::vector<std::string> Collection::getAllKeys() const
{
    std::vector<std::string> result;
    result.reserve(m_key_value.size());
    for (const auto& [key, value] : m_key_value)
    {
        (void)value;
        result.push_back(key);
    }
    return result;
}

void Collection::appendAllKeysAsJsonArray(std::string& out) const
{
    out.push_back('[');
    bool first = true;
    for (const auto& [key, value] : m_key_value)
    {
        (void)value;
        if (!first)
        {
            out.push_back(',');
        }
        first = false;
        out.push_back('"');
        appendJsonEscapedUtf8(out, key);
        out.push_back('"');
    }
    out.push_back(']');
}

void Collection::flushToDisk()
{
    std::lock_guard<std::mutex> locker(m_flushMutex);
    if (m_storage == nullptr || !m_hasNewRecords)
    {
        return;
    }

    const auto startedAt = std::chrono::steady_clock::now();
    if (!m_storage->beginTransaction())
    {
        std::cerr << "Failed to start transaction for flushing collection " << m_name << '\n';
        return;
    }

    bool keepFlushing = false;
    int count = 0;
    for (auto& [doc, records] : m_data)
    {
        for (DataRecord* record : records)
        {
            if (!record->isNew || doc.empty())
            {
                continue;
            }
            if (m_storage->upsertRecord(m_name, doc, record->timestamp, record->data))
            {
                record->isNew = false;
                ++count;
            }
            else
            {
                std::cerr << "Failed to upsert record for collection " << m_name
                          << " doc " << doc << " timestamp " << record->timestamp << '\n';
                keepFlushing = true;
            }
        }
    }

    m_hasNewRecords = keepFlushing;
    if (keepFlushing)
    {
        m_hasNewRecords = false;
        for (const auto& [doc, records] : m_data)
        {
            (void)doc;
            for (const DataRecord* record : records)
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

    if (!m_storage->commitTransaction())
    {
        m_storage->rollbackTransaction();
    }

    const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - startedAt).count();
    std::cerr << "Flushed " << count << " new records to SQLite for collection "
              << m_name << " in " << elapsedNs << " ns\n";
}

void Collection::loadFromDisk()
{
    if (m_storage == nullptr)
    {
        return;
    }

    std::cerr << "Loading collection from SQLite " << m_name << '\n';
    m_data.clear();
    m_key_value.clear();
    m_recordPool.clearFreeList();
    m_hasNewRecords = false;

    const auto records = m_storage->fetchRecords(m_name);
    for (const auto& record : records)
    {
        insertInternal(record.timestamp, record.document, record.data, false);
    }

    const auto keyValues = m_storage->fetchKeyValues(m_name);
    for (const auto& kv : keyValues)
    {
        m_key_value[kv.key] = kv.value;
    }
    std::cerr << "Done loading collection from SQLite " << m_name << '\n';
}
