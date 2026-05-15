#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "collection.h"
#include "messagerequest.h"

class SqliteStorage;

namespace MessageType {
    inline constexpr std::string_view Auth = "auth";
    inline constexpr std::string_view Insert = "ins";
    inline constexpr std::string_view QuerySessions = "qry";
    inline constexpr std::string_view QueryCollections = "cols";
    inline constexpr std::string_view QueryDocument = "qdoc";
    inline constexpr std::string_view DeleteDocument = "ddoc";
    inline constexpr std::string_view DeleteCollection = "dcol";
    inline constexpr std::string_view DeleteRecord = "drec";
    inline constexpr std::string_view DeleteMultipleRecords = "dmrec";
    inline constexpr std::string_view DeleteRecordsRange = "drrng";
    inline constexpr std::string_view SetValue = "sval";
    inline constexpr std::string_view GetValue = "gval";
    inline constexpr std::string_view GetValues = "gvalues";
    inline constexpr std::string_view RemoveValue = "rval";
    inline constexpr std::string_view GetAllValues = "gvals";
    inline constexpr std::string_view GetAllKeys = "gkeys";
    inline constexpr std::string_view ManageApiKey = "keys";
    inline constexpr std::string_view Connections = "conn";
}

class WebSocket
{
public:
    enum class ApiKeyScope {
        ReadOnly,
        ReadWrite,
        ReadWriteDelete
    };

    enum class RequiredPermission {
        None,
        Read,
        Write,
        Delete,
        ManageKeys
    };

    struct ClientInfo {
        std::string id;
        std::string apiKey;
        std::string name;
        std::string ip;
        ApiKeyScope scope = ApiKeyScope::ReadOnly;
        std::int64_t connectedAtMs = 0;
        std::uint64_t messageCount = 0;
    };

    explicit WebSocket(std::string masterKey, std::string dataFolder, int flushIntervalSeconds = 15);
    ~WebSocket();

    void start(std::uint16_t port = 8080);

private:
    using ClientSender = void (*)(void* socket, std::string_view message, bool closeAfterSend);

    void processMessage(ClientInfo& client, void* socket, ClientSender sender, std::string_view message);
    void flushToDisk();
    void startFlushThread();
    void stopFlushThread();

    std::string handleMessage(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleQueryDocument(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleQuerySessions(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleQueryCollections(ClientInfo& client, const MessageRequest& message);
    std::string handleDeleteDocument(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleDeleteCollection(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleDeleteRecord(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleDeleteMultipleRecords(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleDeleteRecordsRange(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleInsert(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleConnections(const ClientInfo& client, const MessageRequest& message);

    std::string handleSetValue(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleGetValue(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleGetValues(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleRemoveValue(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleGetAllValues(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleGetAllKeys(ClientInfo& client, const MessageRequest& message, bool& closeClient);
    std::string handleManageApiKey(ClientInfo& client, const MessageRequest& message, bool& closeClient);

    bool hasPermission(ApiKeyScope scope, RequiredPermission required) const;
    RequiredPermission permissionForType(std::string_view type) const;
    bool registerApiKey(std::string_view key, ApiKeyScope scope, bool deletable, std::string* errorMessage = nullptr, bool persistToStorage = true);
    bool removeApiKey(std::string_view key, std::string* errorMessage = nullptr);
    bool parseScope(std::string_view scopeString, ApiKeyScope* scopeOut) const;
    std::string scopeToString(ApiKeyScope scope) const;
    void loadApiKeysFromDisk();
    Collection* collectionFor(std::string_view name, bool create);

    struct ApiKeyEntry {
        ApiKeyScope scope;
        bool deletable = false;
    };

    ApiKeyEntry* lookupApiKey(std::string_view key);

    static std::int64_t nowMs();
    static void appendJsonString(std::string& out, std::string_view value);
    static void appendRecordAsJson(std::string& out, const DataRecord* record);
    static bool tryParseRegexPattern(std::string_view candidate, std::regex* regexOut);
    static std::string normalizeScope(std::string_view scope);

    std::string m_masterKey;
    std::string m_dataFolder;
    int m_flushIntervalSeconds = 15;

    std::unique_ptr<SqliteStorage> m_storage;
    std::unordered_map<std::string, std::unique_ptr<Collection>> m_databases;
    std::unordered_map<std::string, ApiKeyEntry> m_apiKeys;
    std::unordered_map<std::string, ClientInfo*> m_clients;

    std::mutex m_mutex;
    std::atomic<bool> m_running{false};
    std::thread m_flushThread;
    std::uint64_t m_nextClientId = 1;
};

#endif // WEBSOCKET_H
