#include "websocket.h"

#include <App.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <csignal>
#include <iostream>
#include <sstream>

#include "deletecollection.h"
#include "deletedocument.h"
#include "deletemultiplerecords.h"
#include "deleterecord.h"
#include "deleterecordsrange.h"
#include "insertrequest.h"
#include "json_helpers.h"
#include "keyvalue.h"
#include "querydocument.h"
#include "querysessions.h"
#include "sqlitestorage.h"

namespace {

std::atomic<bool> g_stopRequested{false};

void handleSignal(int)
{
    g_stopRequested.store(true);
}

struct PerSocketData {
    WebSocket::ClientInfo client;
};

using UwsSocket = uWS::WebSocket<false, true, PerSocketData>;

void sendToUwsSocket(void* socket, std::string_view message, bool closeAfterSend)
{
    auto* ws = static_cast<UwsSocket*>(socket);
    if (!message.empty())
    {
        ws->send(message, uWS::OpCode::TEXT);
    }
    if (closeAfterSend)
    {
        ws->end(1008, "policy violation");
    }
}

} // namespace

WebSocket::WebSocket(std::string masterKey, std::string dataFolder, int flushIntervalSeconds)
    : m_masterKey(std::move(masterKey)),
      m_dataFolder(std::move(dataFolder)),
      m_flushIntervalSeconds(flushIntervalSeconds > 0 ? flushIntervalSeconds : 15)
{
    std::string errorMessage;
    if (!registerApiKey(m_masterKey, ApiKeyScope::ReadWriteDelete, false, &errorMessage))
    {
        std::cerr << "Failed to register master API key: " << errorMessage << '\n';
    }

    if (m_dataFolder.empty())
    {
        std::cerr << "Running in non-persistent mode (no data folder specified)\n";
        return;
    }

    m_storage = std::make_unique<SqliteStorage>();
    if (!m_storage->initialize(m_dataFolder))
    {
        throw std::runtime_error("Failed to initialize SQLite storage");
    }

    std::cerr << "Running in persistent mode (data folder specified): " << m_dataFolder << '\n';
    std::cerr << "Flush interval set to " << m_flushIntervalSeconds << " seconds\n";

    loadApiKeysFromDisk();
    for (const std::string& collection : m_storage->collections())
    {
        auto col = std::make_unique<Collection>(collection, m_storage.get());
        col->loadFromDisk();
        m_databases[collection] = std::move(col);
    }

    startFlushThread();
}

WebSocket::~WebSocket()
{
    stopFlushThread();
    flushToDisk();
}

void WebSocket::startFlushThread()
{
    if (m_storage == nullptr || m_running.exchange(true))
    {
        return;
    }

    m_flushThread = std::thread([this]() {
        while (m_running.load())
        {
            for (int i = 0; i < m_flushIntervalSeconds && m_running.load(); ++i)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (m_running.load())
            {
                flushToDisk();
            }
        }
    });
}

void WebSocket::stopFlushThread()
{
    m_running.store(false);
    if (m_flushThread.joinable())
    {
        m_flushThread.join();
    }
}

void WebSocket::flushToDisk()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_storage == nullptr)
    {
        return;
    }

    for (auto& [key, value] : m_databases)
    {
        (void)key;
        value->flushToDisk();
    }
    m_storage->checkpointWal(false);
}

void WebSocket::start(std::uint16_t port)
{
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    uWS::App app;
    app.ws<PerSocketData>("/*", {
        .compression = uWS::SHARED_COMPRESSOR,
        .maxPayloadLength = 16 * 1024 * 1024,
        .idleTimeout = 120,
        .maxBackpressure = 16 * 1024 * 1024,
        .closeOnBackpressureLimit = false,
        .resetIdleTimeoutOnSend = true,
        .sendPingsAutomatically = true,
        .upgrade = [this](auto* res, auto* req, auto* context) {
            const std::string apiKey(req->getQuery("api-key"));
            const std::string clientName(req->getQuery("name"));
            if (apiKey.empty())
            {
                res->writeStatus("401 Unauthorized")->end("Missing API key parameter");
                return;
            }

            ApiKeyEntry* entry = nullptr;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                entry = lookupApiKey(apiKey);
            }
            if (entry == nullptr)
            {
                res->writeStatus("401 Unauthorized")->end("Unknown API key");
                return;
            }

            PerSocketData data;
            data.client.id = std::to_string(m_nextClientId++);
            data.client.apiKey = apiKey;
            data.client.name = clientName;
            data.client.scope = entry->scope;
            data.client.connectedAtMs = nowMs();

            res->template upgrade<PerSocketData>(
                std::move(data),
                req->getHeader("sec-websocket-key"),
                req->getHeader("sec-websocket-protocol"),
                req->getHeader("sec-websocket-extensions"),
                context);
        },
        .open = [this](auto* ws) {
            auto* data = ws->getUserData();
            data->client.ip = std::string(ws->getRemoteAddressAsText());
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_clients[data->client.id] = &data->client;
            }
            std::cerr << "New client connected: " << data->client.ip
                      << " ID " << data->client.id
                      << " Scope " << scopeToString(data->client.scope) << '\n';
            ws->send("{\"message\":\"Authentication successful\",\"type\":\"ready\"}", uWS::OpCode::TEXT);
        },
        .message = [this](auto* ws, std::string_view message, uWS::OpCode opCode) {
            if (opCode != uWS::OpCode::TEXT)
            {
                ws->end(1003, "text frames only");
                return;
            }
            auto* data = ws->getUserData();
            processMessage(data->client, ws, sendToUwsSocket, message);
        },
        .close = [this](auto* ws, int code, std::string_view message) {
            auto* data = ws->getUserData();
            const std::int64_t lifetimeMs = nowMs() - data->client.connectedAtMs;
            std::cerr << "Client disconnected: " << data->client.ip
                      << " ID " << data->client.id
                      << " CloseCode " << code
                      << " CloseReason " << message
                      << " LifetimeMs " << lifetimeMs
                      << " Messages " << data->client.messageCount << '\n';
            std::lock_guard<std::mutex> lock(m_mutex);
            m_clients.erase(data->client.id);
        }
    }).listen(port, [port](auto* token) {
        if (token)
        {
            std::cerr << "WebSocket server listening on port " << port << '\n';
        }
        else
        {
            std::cerr << "Failed to start WebSocket server on port " << port << '\n';
            g_stopRequested.store(true);
        }
    });

    app.run();
}

void WebSocket::processMessage(ClientInfo& client, void* socket, ClientSender sender, std::string_view message)
{
    ++client.messageCount;

    bool ok = false;
    MessageRequest msg = MessageRequest::fromJson(message, &ok);
    if (!ok)
    {
        std::cerr << "Closing client for safety: invalid message from " << client.ip << " ID " << client.id << '\n';
        sender(socket, "", true);
        return;
    }

    if (msg.type == MessageType::Auth)
    {
        Json obj;
        obj["id"] = msg.id;
        obj["error"] = "auth messages are not supported; use the api-key query parameter";
        sender(socket, obj.dump(), false);
        return;
    }

    const RequiredPermission requiredPermission = permissionForType(msg.type);
    if (!hasPermission(client.scope, requiredPermission))
    {
        Json obj;
        obj["id"] = msg.id;
        obj["error"] = "permission denied";
        sender(socket, obj.dump(), false);
        return;
    }

    bool closeClient = false;
    std::string response;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        response = handleMessage(client, msg, closeClient);
    }

    if (!response.empty() || closeClient)
    {
        sender(socket, response, closeClient);
    }
}

std::string WebSocket::handleMessage(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    if (message.type == MessageType::Insert)
    {
        return handleInsert(client, message, closeClient);
    }
    if (message.type == MessageType::QuerySessions)
    {
        return handleQuerySessions(client, message, closeClient);
    }
    if (message.type == MessageType::QueryCollections)
    {
        return handleQueryCollections(client, message);
    }
    if (message.type == MessageType::QueryDocument)
    {
        return handleQueryDocument(client, message, closeClient);
    }
    if (message.type == MessageType::DeleteDocument)
    {
        return handleDeleteDocument(client, message, closeClient);
    }
    if (message.type == MessageType::DeleteCollection)
    {
        return handleDeleteCollection(client, message, closeClient);
    }
    if (message.type == MessageType::DeleteRecord)
    {
        return handleDeleteRecord(client, message, closeClient);
    }
    if (message.type == MessageType::DeleteMultipleRecords)
    {
        return handleDeleteMultipleRecords(client, message, closeClient);
    }
    if (message.type == MessageType::DeleteRecordsRange)
    {
        return handleDeleteRecordsRange(client, message, closeClient);
    }
    if (message.type == MessageType::SetValue)
    {
        return handleSetValue(client, message, closeClient);
    }
    if (message.type == MessageType::GetValue)
    {
        return handleGetValue(client, message, closeClient);
    }
    if (message.type == MessageType::GetValues)
    {
        return handleGetValues(client, message, closeClient);
    }
    if (message.type == MessageType::RemoveValue)
    {
        return handleRemoveValue(client, message, closeClient);
    }
    if (message.type == MessageType::GetAllValues)
    {
        return handleGetAllValues(client, message, closeClient);
    }
    if (message.type == MessageType::GetAllKeys)
    {
        return handleGetAllKeys(client, message, closeClient);
    }
    if (message.type == MessageType::ManageApiKey)
    {
        return handleManageApiKey(client, message, closeClient);
    }
    if (message.type == MessageType::Connections)
    {
        return handleConnections(client, message);
    }

    closeClient = true;
    return "{\"error\":\"Unknown message type\"}";
}

std::string WebSocket::handleInsert(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    std::vector<InsertRequest> payloads = InsertRequest::fromJson(message.data, &ok);
    if (!ok)
    {
        closeClient = true;
        return {};
    }

    for (const InsertRequest& payload : payloads)
    {
        Collection* database = collectionFor(payload.col, true);
        database->insert(payload.ts, payload.doc, payload.data);
    }

    return JsonHelpers::compactObjectWithId(message.id);
}

std::string WebSocket::handleQuerySessions(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    QuerySessions query = QuerySessions::fromJson(message.data, &ok);
    if (!ok)
    {
        closeClient = true;
        return {};
    }

    Collection* database = collectionFor(query.col, false);
    std::string response;
    response.append("{\"id\":");
    appendJsonString(response, message.id);
    response.append(",\"records\":");

    if (database == nullptr)
    {
        response.append("{}");
    }
    else
    {
        std::regex docRegex;
        const bool useRegex = tryParseRegexPattern(query.doc, &docRegex);
        std::regex whereRegex;
        const bool useWhereRegex = tryParseRegexPattern(query.where, &whereRegex);
        std::regex filterRegex;
        const bool useFilterRegex = tryParseRegexPattern(query.filter, &filterRegex);
        auto records = database->getAllRecords(query.ts,
                                               useRegex ? std::string_view() : std::string_view(query.doc),
                                               query.from,
                                               useRegex ? &docRegex : nullptr,
                                               useWhereRegex ? std::string_view() : std::string_view(query.where),
                                               useFilterRegex ? std::string_view() : std::string_view(query.filter),
                                               useWhereRegex ? &whereRegex : nullptr,
                                               useFilterRegex ? &filterRegex : nullptr);

        response.push_back('{');
        bool first = true;
        for (const auto& [key, record] : records)
        {
            if (!first)
            {
                response.push_back(',');
            }
            first = false;
            appendJsonString(response, key);
            response.push_back(':');
            appendRecordAsJson(response, record);
        }
        response.push_back('}');
    }
    response.push_back('}');
    return response;
}

std::string WebSocket::handleQueryCollections(ClientInfo& client, const MessageRequest& message)
{
    (void)client;
    std::string response;
    response.append("{\"id\":");
    appendJsonString(response, message.id);
    response.append(",\"collections\":[");
    bool first = true;
    for (const auto& [key, value] : m_databases)
    {
        (void)value;
        if (!first)
        {
            response.push_back(',');
        }
        first = false;
        appendJsonString(response, key);
    }
    response.append("]}");
    return response;
}

std::string WebSocket::handleQueryDocument(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    QueryDocument queryDocument = QueryDocument::fromJson(message.data, &ok);
    if (!ok)
    {
        closeClient = true;
        return {};
    }

    Collection* database = collectionFor(queryDocument.col, false);
    std::string response;
    response.append("{\"id\":");
    appendJsonString(response, message.id);
    response.append(",\"records\":[");

    if (database != nullptr)
    {
        std::regex whereRegex;
        const bool useWhereRegex = tryParseRegexPattern(queryDocument.where, &whereRegex);
        std::regex filterRegex;
        const bool useFilterRegex = tryParseRegexPattern(queryDocument.filter, &filterRegex);
        auto records = database->getAllRecordsForDocument(queryDocument.doc,
                                                          queryDocument.from,
                                                          queryDocument.to,
                                                          queryDocument.reverse,
                                                          queryDocument.limit,
                                                          useWhereRegex ? std::string_view() : std::string_view(queryDocument.where),
                                                          useFilterRegex ? std::string_view() : std::string_view(queryDocument.filter),
                                                          useWhereRegex ? &whereRegex : nullptr,
                                                          useFilterRegex ? &filterRegex : nullptr);

        bool first = true;
        for (const DataRecord* record : records)
        {
            if (!first)
            {
                response.push_back(',');
            }
            first = false;
            appendRecordAsJson(response, record);
        }
    }
    response.append("]}");
    return response;
}

std::string WebSocket::handleDeleteDocument(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    DeleteDocument query = DeleteDocument::fromJson(message.data, &ok);
    if (!ok)
    {
        closeClient = true;
        return {};
    }

    if (query.col.empty())
    {
        std::vector<std::string> toErase;
        for (auto& [key, value] : m_databases)
        {
            value->clearDocument(query.doc);
            if (value->isEmpty())
            {
                toErase.push_back(key);
            }
        }
        for (const std::string& key : toErase)
        {
            m_databases.erase(key);
            if (m_storage != nullptr)
            {
                m_storage->deleteCollection(key);
            }
        }
    }
    else
    {
        Collection* database = collectionFor(query.col, false);
        if (database != nullptr)
        {
            database->clearDocument(query.doc);
            if (database->isEmpty())
            {
                m_databases.erase(query.col);
                if (m_storage != nullptr)
                {
                    m_storage->deleteCollection(query.col);
                }
            }
        }
    }
    return JsonHelpers::compactObjectWithId(message.id);
}

std::string WebSocket::handleDeleteCollection(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    DeleteCollection query = DeleteCollection::fromJson(message.data, &ok);
    if (!ok)
    {
        closeClient = true;
        return {};
    }

    m_databases.erase(query.col);
    if (m_storage != nullptr)
    {
        m_storage->deleteCollection(query.col);
    }
    return JsonHelpers::compactObjectWithId(message.id);
}

std::string WebSocket::handleDeleteRecord(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    DeleteRecord query = DeleteRecord::fromJson(message.data, &ok);
    if (!ok || !query.isValid())
    {
        closeClient = true;
        return {};
    }

    Collection* database = collectionFor(query.col, false);
    if (database != nullptr)
    {
        database->deleteRecord(query.doc, query.ts);
    }
    return JsonHelpers::compactObjectWithId(message.id);
}

std::string WebSocket::handleDeleteMultipleRecords(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    DeleteMultipleRecords query = DeleteMultipleRecords::fromJson(message.data, &ok);
    if (!ok)
    {
        closeClient = true;
        return {};
    }

    for (const DeleteRecord& record : query.records)
    {
        Collection* database = collectionFor(record.col, false);
        if (database != nullptr)
        {
            database->deleteRecord(record.doc, record.ts);
        }
    }
    return JsonHelpers::compactObjectWithId(message.id);
}

std::string WebSocket::handleDeleteRecordsRange(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    DeleteRecordsRange query = DeleteRecordsRange::fromJson(message.data, &ok);
    if (!ok || !query.isValid())
    {
        closeClient = true;
        return {};
    }

    Collection* database = collectionFor(query.col, false);
    if (database != nullptr)
    {
        database->deleteRecordsInRange(query.doc, query.fromTs, query.toTs);
    }
    return JsonHelpers::compactObjectWithId(message.id);
}

std::string WebSocket::handleSetValue(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    KeyValue kv = KeyValue::fromJson(message.data, &ok);
    if (!ok || !kv.isValid() || !kv.hasKey() || !kv.hasValue())
    {
        closeClient = true;
        return {};
    }

    Collection* database = collectionFor(kv.col, true);
    database->setValueForKey(kv.key, kv.value);
    return JsonHelpers::compactObjectWithId(message.id);
}

std::string WebSocket::handleGetValue(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    KeyValue kv = KeyValue::fromJson(message.data, &ok);
    if (!ok || !kv.isValid() || !kv.hasKey())
    {
        closeClient = true;
        return {};
    }

    Collection* database = collectionFor(kv.col, false);
    std::string response;
    response.append("{\"id\":");
    appendJsonString(response, message.id);
    response.append(",\"value\":\"");
    if (database != nullptr)
    {
        const std::string* value = database->getValueRefForKey(kv.key);
        if (value != nullptr)
        {
            appendJsonEscapedUtf8(response, *value);
        }
    }
    response.append("\"}");
    return response;
}

std::string WebSocket::handleGetValues(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    KeyValue kv = KeyValue::fromJson(message.data, &ok);
    if (!ok || !kv.isValid() || !kv.hasKey())
    {
        closeClient = true;
        return {};
    }

    Collection* database = collectionFor(kv.col, false);
    std::string response;
    response.append("{\"id\":");
    appendJsonString(response, message.id);
    response.append(",\"values\":");

    if (database == nullptr)
    {
        response.append("{}");
    }
    else
    {
        std::regex keyRegex;
        if (tryParseRegexPattern(kv.key, &keyRegex))
        {
            database->appendAllValuesAsJson(response, &keyRegex);
        }
        else
        {
            response.push_back('{');
            appendJsonString(response, kv.key);
            response.append(":\"");
            const std::string* value = database->getValueRefForKey(kv.key);
            if (value != nullptr)
            {
                appendJsonEscapedUtf8(response, *value);
            }
            response.append("\"}");
        }
    }
    response.push_back('}');
    return response;
}

std::string WebSocket::handleRemoveValue(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    KeyValue kv = KeyValue::fromJson(message.data, &ok);
    if (!ok || !kv.isValid() || !kv.hasKey())
    {
        closeClient = true;
        return {};
    }

    Collection* database = collectionFor(kv.col, false);
    if (database != nullptr)
    {
        database->removeValueForKey(kv.key);
    }
    return JsonHelpers::compactObjectWithId(message.id);
}

std::string WebSocket::handleGetAllValues(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    KeyValue kv = KeyValue::fromJson(message.data, &ok);
    if (!ok || !kv.isValid())
    {
        closeClient = true;
        return {};
    }

    Collection* database = collectionFor(kv.col, false);
    std::string response;
    response.append("{\"id\":");
    appendJsonString(response, message.id);
    response.append(",\"values\":");
    if (database == nullptr)
    {
        response.append("{}");
    }
    else
    {
        database->appendAllValuesAsJson(response);
    }
    response.push_back('}');
    return response;
}

std::string WebSocket::handleGetAllKeys(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    (void)client;
    bool ok = false;
    KeyValue kv = KeyValue::fromJson(message.data, &ok);
    if (!ok || !kv.isValid())
    {
        closeClient = true;
        return {};
    }

    Collection* database = collectionFor(kv.col, false);
    std::string response;
    response.append("{\"id\":");
    appendJsonString(response, message.id);
    response.append(",\"keys\":");
    if (database == nullptr)
    {
        response.append("[]");
    }
    else
    {
        database->appendAllKeysAsJsonArray(response);
    }
    response.push_back('}');
    return response;
}

std::string WebSocket::handleConnections(const ClientInfo& client, const MessageRequest& message)
{
    const std::int64_t now = nowMs();
    std::string response;
    response.append("{\"id\":");
    appendJsonString(response, message.id);
    response.append(",\"connections\":[");
    bool first = true;
    for (const auto& [id, info] : m_clients)
    {
        if (info == nullptr)
        {
            continue;
        }
        if (!first)
        {
            response.push_back(',');
        }
        first = false;
        response.append("{\"ip\":");
        appendJsonString(response, info->ip);
        response.append(",\"since\":");
        response.append(std::to_string(now - info->connectedAtMs));
        response.append(",\"name\":");
        if (info->name.empty())
        {
            response.append("null");
        }
        else
        {
            appendJsonString(response, info->name);
        }
        response.append(",\"self\":");
        response.append(info->id == client.id ? "true" : "false");
        response.push_back('}');
    }
    response.append("]}");
    return response;
}

std::string WebSocket::handleManageApiKey(ClientInfo& client, const MessageRequest& message, bool& closeClient)
{
    if (client.apiKey != m_masterKey)
    {
        Json response;
        response["id"] = message.id;
        response["error"] = "only the master key may manage API keys";
        return response.dump();
    }

    Json payload;
    if (!JsonHelpers::parse(message.data, payload) || !payload.is_object())
    {
        closeClient = true;
        return {};
    }

    const std::string action = normalizeScope(JsonHelpers::stringValue(payload, "action"));
    Json response;
    response["id"] = message.id;

    if (action == "add")
    {
        const std::string key = JsonHelpers::stringValue(payload, "key");
        const std::string scopeString = JsonHelpers::stringValue(payload, "scope");
        ApiKeyScope scopeValue;
        if (!parseScope(scopeString, &scopeValue))
        {
            response["error"] = "invalid scope";
        }
        else
        {
            std::string errorMessage;
            if (registerApiKey(key, scopeValue, true, &errorMessage))
            {
                response["status"] = "ok";
                response["scope"] = scopeToString(scopeValue);
            }
            else
            {
                response["error"] = errorMessage;
            }
        }
    }
    else if (action == "remove")
    {
        const std::string key = JsonHelpers::stringValue(payload, "key");
        std::string errorMessage;
        if (removeApiKey(key, &errorMessage))
        {
            response["status"] = "ok";
        }
        else
        {
            response["error"] = errorMessage;
        }
    }
    else if (action == "list")
    {
        response["status"] = "ok";
        response["keys"] = Json::array();
        for (const auto& [key, entry] : m_apiKeys)
        {
            response["keys"].push_back({
                {"key", key},
                {"scope", scopeToString(entry.scope)},
                {"deletable", entry.deletable},
            });
        }
    }
    else
    {
        response["error"] = "unknown action";
    }

    return response.dump();
}

bool WebSocket::hasPermission(ApiKeyScope scope, RequiredPermission required) const
{
    if (required == RequiredPermission::None)
    {
        return true;
    }

    switch (scope)
    {
    case ApiKeyScope::ReadOnly:
        return required == RequiredPermission::Read;
    case ApiKeyScope::ReadWrite:
        return required == RequiredPermission::Read || required == RequiredPermission::Write;
    case ApiKeyScope::ReadWriteDelete:
        return true;
    }
    return false;
}

WebSocket::RequiredPermission WebSocket::permissionForType(std::string_view type) const
{
    if (type == MessageType::Insert || type == MessageType::SetValue)
    {
        return RequiredPermission::Write;
    }
    if (type == MessageType::QuerySessions || type == MessageType::QueryCollections ||
        type == MessageType::QueryDocument || type == MessageType::GetValue ||
        type == MessageType::GetValues || type == MessageType::Connections ||
        type == MessageType::GetAllValues || type == MessageType::GetAllKeys)
    {
        return RequiredPermission::Read;
    }
    if (type == MessageType::DeleteDocument || type == MessageType::DeleteCollection ||
        type == MessageType::DeleteRecord || type == MessageType::DeleteMultipleRecords ||
        type == MessageType::DeleteRecordsRange || type == MessageType::RemoveValue)
    {
        return RequiredPermission::Delete;
    }
    if (type == MessageType::ManageApiKey)
    {
        return RequiredPermission::ManageKeys;
    }
    return RequiredPermission::None;
}

bool WebSocket::registerApiKey(std::string_view key, ApiKeyScope scope, bool deletable, std::string* errorMessage, bool persistToStorage)
{
    if (key.empty())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = "api key cannot be empty";
        }
        return false;
    }

    const bool isMaster = key == m_masterKey;
    const ApiKeyScope scopeToStore = isMaster ? ApiKeyScope::ReadWriteDelete : scope;
    const bool deletableToStore = isMaster ? false : deletable;
    const std::string keyString(key);

    auto it = m_apiKeys.find(keyString);
    if (it == m_apiKeys.end())
    {
        m_apiKeys.emplace(keyString, ApiKeyEntry{scopeToStore, deletableToStore});
    }
    else
    {
        it->second.scope = scopeToStore;
        it->second.deletable = it->second.deletable ? deletableToStore : false;
    }

    for (auto& [id, info] : m_clients)
    {
        (void)id;
        if (info != nullptr && info->apiKey == key)
        {
            info->scope = scopeToStore;
        }
    }

    if (!isMaster && m_storage != nullptr && persistToStorage)
    {
        if (!m_storage->upsertApiKey(key, scopeToString(scopeToStore), deletableToStore))
        {
            std::cerr << "Failed to persist API key to SQLite\n";
        }
    }

    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }
    return true;
}

bool WebSocket::removeApiKey(std::string_view key, std::string* errorMessage)
{
    auto it = m_apiKeys.find(std::string(key));
    if (it == m_apiKeys.end())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = "api key not found";
        }
        return false;
    }
    if (!it->second.deletable)
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = "api key cannot be removed";
        }
        return false;
    }

    m_apiKeys.erase(it);
    for (auto& [id, info] : m_clients)
    {
        (void)id;
        if (info != nullptr && info->apiKey == key)
        {
            info->scope = ApiKeyScope::ReadOnly;
        }
    }

    if (m_storage != nullptr && key != m_masterKey)
    {
        if (!m_storage->deleteApiKey(key))
        {
            std::cerr << "Failed to delete API key from SQLite\n";
        }
    }

    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }
    return true;
}

void WebSocket::loadApiKeysFromDisk()
{
    if (m_storage == nullptr)
    {
        std::cerr << "SQLite storage unavailable, API keys will not persist\n";
        return;
    }

    int loadedCount = 0;
    for (const auto& row : m_storage->fetchApiKeys())
    {
        ApiKeyScope scope;
        if (!parseScope(row.scope, &scope))
        {
            std::cerr << "Invalid scope for persisted API key, skipping: " << row.key << '\n';
            continue;
        }

        if (row.key == m_masterKey)
        {
            scope = ApiKeyScope::ReadWriteDelete;
        }

        std::string errorMessage;
        if (registerApiKey(row.key, scope, row.deletable, &errorMessage, false))
        {
            ++loadedCount;
        }
        else
        {
            std::cerr << "Failed to load API key: " << row.key << ' ' << errorMessage << '\n';
        }
    }
    std::cerr << "Loaded " << loadedCount << " API keys from SQLite\n";
}

bool WebSocket::parseScope(std::string_view scopeString, ApiKeyScope* scopeOut) const
{
    if (scopeOut == nullptr)
    {
        return false;
    }

    const std::string normalized = normalizeScope(scopeString);
    if (normalized == "readonly")
    {
        *scopeOut = ApiKeyScope::ReadOnly;
        return true;
    }
    if (normalized == "readwrite")
    {
        *scopeOut = ApiKeyScope::ReadWrite;
        return true;
    }
    if (normalized == "readwritedelete")
    {
        *scopeOut = ApiKeyScope::ReadWriteDelete;
        return true;
    }
    return false;
}

std::string WebSocket::scopeToString(ApiKeyScope scope) const
{
    switch (scope)
    {
    case ApiKeyScope::ReadOnly:
        return "readonly";
    case ApiKeyScope::ReadWrite:
        return "read_write";
    case ApiKeyScope::ReadWriteDelete:
        return "read_write_delete";
    }
    return "unknown";
}

Collection* WebSocket::collectionFor(std::string_view name, bool create)
{
    const std::string key(name);
    auto it = m_databases.find(key);
    if (it != m_databases.end())
    {
        return it->second.get();
    }
    if (!create)
    {
        return nullptr;
    }

    auto collection = std::make_unique<Collection>(key, m_storage.get());
    Collection* raw = collection.get();
    m_databases.emplace(key, std::move(collection));
    return raw;
}

WebSocket::ApiKeyEntry* WebSocket::lookupApiKey(std::string_view key)
{
    auto it = m_apiKeys.find(std::string(key));
    return it == m_apiKeys.end() ? nullptr : &it->second;
}

std::int64_t WebSocket::nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void WebSocket::appendJsonString(std::string& out, std::string_view value)
{
    out.push_back('"');
    appendJsonEscapedUtf8(out, value);
    out.push_back('"');
}

void WebSocket::appendRecordAsJson(std::string& out, const DataRecord* record)
{
    out.append("{\"ts\":");
    out.append(std::to_string(record->timestamp));
    out.append(",\"data\":\"");
    appendJsonEscapedUtf8(out, record->data);
    out.append("\"}");
}

bool WebSocket::tryParseRegexPattern(std::string_view candidate, std::regex* regexOut)
{
    if (candidate.size() < 2 || candidate.front() != '/')
    {
        return false;
    }

    std::size_t closingSlashIndex = std::string_view::npos;
    bool escaping = false;
    for (std::size_t i = 1; i < candidate.size(); ++i)
    {
        const char ch = candidate[i];
        if (!escaping && ch == '/')
        {
            closingSlashIndex = i;
            break;
        }
        if (!escaping && ch == '\\')
        {
            escaping = true;
            continue;
        }
        escaping = false;
    }

    if (closingSlashIndex == std::string_view::npos)
    {
        return false;
    }

    const std::string pattern(candidate.substr(1, closingSlashIndex - 1));
    const std::string_view flags = candidate.substr(closingSlashIndex + 1);
    std::regex::flag_type options = std::regex::ECMAScript;
    for (char flag : flags)
    {
        if (flag == 'i')
        {
            options |= std::regex::icase;
        }
        else if (flag == 'm' || flag == 's')
        {
            continue;
        }
        else
        {
            std::cerr << "Invalid regex flag " << flag << " in pattern " << candidate << '\n';
            return false;
        }
    }

    try
    {
        if (regexOut != nullptr)
        {
            *regexOut = std::regex(pattern, options);
        }
        return true;
    }
    catch (const std::regex_error& error)
    {
        std::cerr << "Invalid regex pattern " << candidate << ": " << error.what() << '\n';
        return false;
    }
}

std::string WebSocket::normalizeScope(std::string_view scope)
{
    std::string normalized;
    normalized.reserve(scope.size());
    for (unsigned char c : scope)
    {
        if (c == ' ' || c == ',' || c == '-' || c == '_')
        {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(c)));
    }
    return normalized;
}
