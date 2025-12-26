#include "websocket.h"
#include <QDebug>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTime>
#include <QElapsedTimer>
#include <QUuid>
#include <QUrl>
#include <QUrlQuery>
#include <QWebSocketProtocol>
#include <QRegularExpression>
#include "insertrequest.h"
#include "querysessions.h"
#include "querydocument.h"
#include "deletedocument.h"
#include "deletecollection.h"
#include "keyvalue.h"
#include "deleterecord.h"
#include "deletemultiplerecords.h"
#include "deleterecordsrange.h"
#include "sqlitestorage.h"

namespace {

bool tryParseRegexPattern(const QString &candidate, QRegularExpression *regexOut)
{
    if (candidate.size() < 2 || !candidate.startsWith('/'))
    {
        return false;
    }

    int closingSlashIndex = -1;
    bool escaping = false;
    for (int i = 1; i < candidate.size(); ++i)
    {
        const QChar ch = candidate.at(i);
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

    if (closingSlashIndex == -1)
    {
        return false;
    }

    const QString pattern = candidate.mid(1, closingSlashIndex - 1);
    const QString flags = candidate.mid(closingSlashIndex + 1);

    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    for (const QChar flag : flags)
    {
        if (flag == 'i')
        {
            options |= QRegularExpression::CaseInsensitiveOption;
        }
        else if (flag == 'm')
        {
            options |= QRegularExpression::MultilineOption;
        }
        else if (flag == 's')
        {
            options |= QRegularExpression::DotMatchesEverythingOption;
        }
    }

    QRegularExpression regex(pattern, options);
    if (!regex.isValid())
    {
        qWarning() << "Invalid regex pattern" << candidate << ":" << regex.errorString();
        return false;
    }

    if (regexOut != nullptr)
    {
        *regexOut = regex;
    }

    return true;
}

} // namespace


WebSocket::WebSocket(const QString &masterKey, const QString &dataFolder, int flushIntervalSeconds, QObject *parent) : QObject(parent)
{
    m_masterKey = masterKey;
    m_dataFolder = dataFolder;
    m_server = new QWebSocketServer(QStringLiteral("WebSocket Server"), QWebSocketServer::NonSecureMode, this);

    QString errorMessage;
    if (!registerApiKey(m_masterKey, ApiKeyScope::ReadWriteDelete, false, &errorMessage)) {
        qWarning() << "Failed to register master API key:" << errorMessage;
    }
    
    if (m_dataFolder.isEmpty()) {
        qInfo() << "Running in non-persistent mode (no data folder specified)";
        return;
    }
    m_storage = std::make_unique<SqliteStorage>();
    if (!m_storage->initialize(m_dataFolder))
    {
        qFatal("Failed to initialize SQLite storage");
    }
    qInfo() << "Running in persistent mode (data folder specified):" << m_dataFolder;
    qInfo() << "Flush interval set to" << flushIntervalSeconds << "seconds";
    m_flushTimer.start(flushIntervalSeconds * 1000);
    connect(&m_flushTimer, &QTimer::timeout, this, &WebSocket::flushToDisk);

    // Load API keys from storage
    loadApiKeysFromDisk();

    // Load existing collections from SQLite
    if (m_storage != nullptr)
    {
        const QStringList storedCollections = m_storage->collections();
        for (const QString &collection : storedCollections)
        {
            auto col = std::make_unique<Collection>(collection, m_storage.get());
            col->loadFromDisk();
            m_databases[collection] = std::move(col);
        }
    }
}

WebSocket::~WebSocket()
{
    m_server->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

void WebSocket::flushToDisk()
{
    if (m_storage == nullptr) {
        return; // Skip persistence if no storage specified
    }
    
    for (auto &[key, value] : m_databases)
    {
        value->flushToDisk();
    }
}

void WebSocket::start(quint16 port)
{
    if (m_server->listen(QHostAddress::Any, port))
    {
        qInfo() << QTime::currentTime().toString() << "WebSocket server listening on port" << port;
        connect(m_server, &QWebSocketServer::newConnection, this, &WebSocket::onNewConnection);
    }
    else
    {
        qFatal("Failed to start WebSocket server");
    }
}

void WebSocket::onNewConnection()
{
    QWebSocket *socket = m_server->nextPendingConnection();
    if (!socket)
    {
        return;
    }

    socket->setObjectName(QUuid::createUuid().toString());
    
    // In Qt 6.4, QWebSocketHandshakeRequest is not available
    // Extract API key from the request URL query parameters
    const QUrl requestUrl = socket->requestUrl();
    const QUrlQuery query(requestUrl);
    const QString apiKey = query.queryItemValue("api-key");
    const QString clientName = query.queryItemValue("name");

    if (apiKey.isEmpty())
    {
        qWarning() << QTime::currentTime().toString() << "Missing API key parameter from" << socket->peerAddress().toString();
        rejectClient(socket, QStringLiteral("Missing API key parameter"));
        return;
    }

    ApiKeyEntry *entry = lookupApiKey(apiKey);
    if (entry == nullptr)
    {
        qWarning() << QTime::currentTime().toString() << "Unknown API key from" << socket->peerAddress().toString();
        rejectClient(socket, QStringLiteral("Unknown API key"));
        return;
    }

    m_clientScopes[socket->objectName()] = entry->scope;
    m_clientKeys[socket->objectName()] = apiKey;
    m_clientNames[socket->objectName()] = clientName;

    qInfo() << QTime::currentTime().toString() << "New client connected:" << socket->peerAddress().toString()
            << "ID" << socket->objectName() << "Scope" << scopeToString(entry->scope);
    connect(socket, &QWebSocket::textMessageReceived, this, &WebSocket::processMessage);
    connect(socket, &QWebSocket::disconnected, this, &WebSocket::socketDisconnected);
    m_clients << socket;
    m_connectionTimes[socket->objectName()] = QDateTime::currentMSecsSinceEpoch();

    // Send authentication success message
    QJsonObject readyMessage;
    readyMessage["type"] = "ready";
    readyMessage["message"] = "Authentication successful";
    QJsonDocument doc(readyMessage);
    socket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
}

void WebSocket::processMessage(const QString &message)
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (!client) { return; }
        
    bool ok;
    MessageRequest msg = MessageRequest::fromJson(message, &ok);

    if (!ok)
    {
        qWarning() << QTime::currentTime().toString() << "Invalid message" << message;
        client->sendTextMessage("");
        client->close();
        return;
    }

    if (msg.type == MessageType::Auth)
    {
        QJsonObject obj;
        obj["id"] = msg.id;
        obj["error"] = "auth messages are not supported; use the x-api-key header";
        client->sendTextMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        return;
    }

    auto scopeIt = m_clientScopes.find(client->objectName());
    if (scopeIt == m_clientScopes.end())
    {
        qWarning() << QTime::currentTime().toString() << "Client with no registered scope:" << client->peerAddress().toString();
        rejectClient(client, QStringLiteral("Authentication required"));
        return;
    }

    RequiredPermission requiredPermission = permissionForType(msg.type);
    if (!hasPermission(scopeIt->second, requiredPermission))
    {
        qWarning() << QTime::currentTime().toString() << "Permission denied for client" << client->peerAddress().toString()
                   << "ID" << client->objectName() << "Type" << msg.type;
        QJsonObject obj;
        obj["id"] = msg.id;
        obj["error"] = "permission denied";
        client->sendTextMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        return;
    }

    handleMessage(client, msg);
}

void WebSocket::handleMessage(QWebSocket *client, const MessageRequest &message)
{
    QString response;
       
    if (message.type == MessageType::Insert)
    {
        response = handleInsert(client, message);
    }
    else if (message.type == MessageType::QuerySessions)
    {
        response = handleQuerySessions(client, message);
    }
    else if (message.type == MessageType::QueryCollections)
    {
        response = handleQueryCollections(client, message);
    }
    else if (message.type == MessageType::QueryDocument)
    {
        response = handleQueryDocument(client, message);
    }
    else if (message.type == MessageType::DeleteDocument)
    {
        response = handleDeleteDocument(client, message);
    }
    else if (message.type == MessageType::DeleteCollection)
    {
        response = handleDeleteCollection(client, message);
    }
    else if (message.type == MessageType::DeleteRecord)
    {
        response = handleDeleteRecord(client, message);
    }
    else if (message.type == MessageType::DeleteMultipleRecords)
    {
        response = handleDeleteMultipleRecords(client, message);
    }
    else if (message.type == MessageType::DeleteRecordsRange)
    {
        response = handleDeleteRecordsRange(client, message);
    }
    else if (message.type == MessageType::SetValue)
    {
        response = handleSetValue(client, message);
    }
    else if (message.type == MessageType::GetValue)
    {
        response = handleGetValue(client, message);
    }
    else if (message.type == MessageType::GetValues)
    {
        response = handleGetValues(client, message);
    }
    else if (message.type == MessageType::RemoveValue)
    {
        response = handleRemoveValue(client, message);
    }
    else if (message.type == MessageType::GetAllValues)
    {
        response = handleGetAllValues(client, message);
    }
    else if (message.type == MessageType::GetAllKeys)
    {
        response = handleGetAllKeys(client, message);
    }
    else if (message.type == MessageType::ManageApiKey)
    {
        response = handleManageApiKey(client, message);
    }
    else if (message.type == MessageType::Connections)
    {
        response = handleConnections(client, message);
    }
    else
    {
        qWarning() << "Unknown message type:" << message.type;
        client->sendTextMessage("{\"error\": \"Unknown message type\"}");
        client->close();
        return;
    }
    if (!response.isEmpty()) {
        if (client->state() != QAbstractSocket::ConnectedState)
        {
            qWarning() << QTime::currentTime().toString() << "Client disconnected:" << client->peerAddress().toString() << "ID" << client->objectName();
            return;
        }
        client->sendTextMessage(response);
    }
}

QString WebSocket::handleInsert(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    QList<InsertRequest> payloads = InsertRequest::fromJson(message.data, &ok);
    if (!ok)
    {
        qWarning() << "Invalid insert message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }

    foreach (InsertRequest payload, payloads)
    {
        auto database = m_databases[payload.col].get();
        if (database == nullptr)
        {
            m_databases[payload.col] = std::make_unique<Collection>(payload.col, m_storage.get());
            database = m_databases[payload.col].get();
        }
        database->insert(payload.ts, payload.doc, payload.data);
    }

    QJsonObject obj;
    obj["id"] = message.id;
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleQuerySessions(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    QuerySessions query = QuerySessions::fromJson(message.data, &ok);
    if (!ok)
    {
        qWarning() << "Invalid query sessions message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }
    auto database = m_databases[query.col].get();
    QJsonObject dataObj;
    QJsonObject obj;
    obj["id"] = message.id;
    if (database == nullptr)
    {
        m_databases.erase(query.col);   
        obj["records"] = QJsonObject();
        QJsonDocument doc(obj);
        return doc.toJson(QJsonDocument::Compact);
    }
    QRegularExpression docRegex;
    const bool useRegex = tryParseRegexPattern(query.doc, &docRegex);
    auto records = database->getAllRecords(query.ts, useRegex ? QString() : query.doc, query.from, useRegex ? &docRegex : nullptr);

    foreach (const QString &key, records.keys())
    {
        dataObj[key] = records[key]->toJson();
    }

    obj["records"] = dataObj;

    QJsonDocument doc(obj);
    auto resp = doc.toJson(QJsonDocument::Compact);
    return resp;
}

QString WebSocket::handleQueryCollections(QWebSocket *client, const MessageRequest &message)
{
    QJsonObject obj;
    obj["id"] = message.id;

    QJsonArray recordsArray;
    for( auto &[key, value] : m_databases)
    {
        recordsArray.append(key);
    }
    obj["collections"] = recordsArray;
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);

}

QString WebSocket::handleQueryDocument(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    QueryDocument queryDocument = QueryDocument::fromJson(message.data, &ok);
    if (!ok)
    {
        qWarning() << "Invalid query document message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }

    QJsonObject dataObj;
    auto database = m_databases[queryDocument.col].get();
    QJsonArray recordsArray;

    dataObj["id"] = message.id;
    if (database == nullptr)
    {
        m_databases.erase(queryDocument.col);   
        dataObj["records"] = recordsArray;
        QJsonDocument doc(dataObj);
        return doc.toJson(QJsonDocument::Compact);
    }
    auto records = database->getAllRecordsForDocument(queryDocument.doc, queryDocument.from, queryDocument.to, queryDocument.reverse, queryDocument.limit);

    foreach (const DataRecord *record, records)
    {
        recordsArray.append(record->toJson());
    }
    dataObj["records"] = recordsArray;

    QJsonDocument doc(dataObj);
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleDeleteDocument(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    DeleteDocument query = DeleteDocument::fromJson(message.data  , &ok);
    if (!ok)
    {
        qWarning() << "Invalid delete document message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }
    QJsonObject obj;
    obj["id"] = message.id;
    QJsonDocument doc(obj);

    if (query.col.isEmpty())
    {
        // Hidden capability: empty collection deletes this document across all collections; SDKs keep this private.
        QVector<QString> toErase;
        
        for (auto &[key, value] : m_databases)
        {
            value->clearDocument(query.doc);
            if (value->isEmpty())
            {
                toErase.append(key);
            }
        }
        
        // Now erase the empty collections
        foreach (const QString &key, toErase)
        {            
            qInfo() << "Deleting collection (1) since there are no more documents:" << key;
            m_databases.erase(key);
            if (m_storage != nullptr)
            {
                m_storage->deleteCollection(key);
            }
        }
    }
    else
    {
        auto database = m_databases[query.col].get();
        if (database == nullptr)
        {
            m_databases.erase(query.col);   
            qWarning() << "Collection not found for collection:" << query.col;
            return doc.toJson(QJsonDocument::Compact);
        }
        database->clearDocument(query.doc);
        
        if (database->isEmpty())
        {
            qInfo() << "Deleting collection (2) since there are no more documents:" << query.col;
            m_databases.erase(query.col);
            if (m_storage != nullptr)
            {
                m_storage->deleteCollection(query.col);
            }
        }
    }
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleDeleteCollection(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    DeleteCollection query = DeleteCollection::fromJson(message.data, &ok);
    if (!ok)
    {
        qWarning() << "Invalid delete collection message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }
    if (m_databases.find(query.col) != m_databases.end())
    {
        m_databases.erase(query.col);
    }
    if (m_storage != nullptr)
    {
        m_storage->deleteCollection(query.col);
    }

    QJsonObject obj;
    obj["id"] = message.id;
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleDeleteRecord(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    DeleteRecord query = DeleteRecord::fromJson(message.data, &ok);
    if (!ok)
    {
        qWarning() << "Invalid delete record message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }
    QJsonObject obj;
    obj["id"] = message.id;
    QJsonDocument doc(obj);

    auto database = m_databases[query.col].get();
    if (database == nullptr)
    {        m_databases.erase(query.col);
        return doc.toJson(QJsonDocument::Compact);
    }
    database->deleteRecord(query.doc, query.ts);    
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleDeleteMultipleRecords(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    DeleteMultipleRecords query = DeleteMultipleRecords::fromJson(message.data, &ok);
    if (!ok)
    {
        qWarning() << "Invalid delete multiple records message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }

    QJsonObject obj;
    obj["id"] = message.id;
    QJsonDocument doc(obj);
    foreach (const DeleteRecord &record, query.records)
    {
        auto database = m_databases[record.col].get();
        if (database == nullptr)
        {
            m_databases.erase(record.col);   
        } else {
            database->deleteRecord(record.doc, record.ts);
        }
    }
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleDeleteRecordsRange(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    DeleteRecordsRange query = DeleteRecordsRange::fromJson(message.data, &ok);
    if (!ok || !query.isValid())
    {
        qWarning() << "Invalid delete records range message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }
    
    QJsonObject obj;
    obj["id"] = message.id;
    QJsonDocument doc(obj);

    auto database = m_databases[query.col].get();
    if (database == nullptr)
    {
        m_databases.erase(query.col);
        return doc.toJson(QJsonDocument::Compact);
    }
    database->deleteRecordsInRange(query.doc, query.fromTs, query.toTs);
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleSetValue(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    KeyValue kv = KeyValue::fromJson(message.data, &ok);
    if (!ok || !kv.isValid() || !kv.hasKey() || !kv.hasValue())
    {
        qWarning() << "Invalid set value message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }

    auto collection = kv.col;
    auto database = m_databases[collection].get();
    if (database == nullptr)
    {
        m_databases[collection] = std::make_unique<Collection>(collection, m_storage.get());
        database = m_databases[collection].get();
    }
    
    database->setValueForKey(kv.key, kv.value);

    QJsonObject obj;
    obj["id"] = message.id;
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleGetValue(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    KeyValue kv = KeyValue::fromJson(message.data, &ok);
    if (!ok || !kv.isValid() || !kv.hasKey())
    {
        qWarning() << "Invalid get value message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }

    QJsonObject obj;
    obj["id"] = message.id;
    obj["value"] = "";

    auto collection = kv.col;
    auto database = m_databases[collection].get();
    
    if (database == nullptr) {
        m_databases.erase(collection);
    } else {
        obj["value"] = database->getValueForKey(kv.key);
    }
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleGetValues(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    KeyValue kv = KeyValue::fromJson(message.data, &ok);
    if (!ok || !kv.isValid() || !kv.hasKey())
    {
        qWarning() << "Invalid get values message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }

    QJsonObject obj;
    obj["id"] = message.id;
    QJsonObject valuesObj;

    auto collection = kv.col;
    auto database = m_databases[collection].get();

    if (database == nullptr)
    {
        m_databases.erase(collection);
    }
    else
    {
        QRegularExpression keyRegex;
        const bool useRegex = tryParseRegexPattern(kv.key, &keyRegex);
        if (useRegex)
        {
            auto values = database->getAllValues(&keyRegex);
            for (auto it = values.constBegin(); it != values.constEnd(); ++it)
            {
                valuesObj[it.key()] = it.value();
            }
        }
        else
        {
            valuesObj[kv.key] = database->getValueForKey(kv.key);
        }
    }

    obj["values"] = valuesObj;
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleRemoveValue(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    KeyValue kv = KeyValue::fromJson(message.data, &ok);
    if (!ok || !kv.isValid() || !kv.hasKey())
    {
        qWarning() << "Invalid remove value message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }

    auto collection = kv.col;
    auto database = m_databases[collection].get();
    if (database == nullptr) {
        m_databases.erase(collection);        
    } else {
        database->removeValueForKey(kv.key);
    }

    QJsonObject obj;
    obj["id"] = message.id;
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleGetAllValues(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    KeyValue kv = KeyValue::fromJson(message.data, &ok);
    if (!ok || !kv.isValid())
    {
        qWarning() << "Invalid get all values message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }

    auto collection = kv.col;
    auto database = m_databases[collection].get();
    QJsonObject valuesObj;
    
    if (database == nullptr) {
        m_databases.erase(collection);
    } else {
        auto values = database->getAllValues();
        for (auto it = values.begin(); it != values.end(); ++it)
        {
            valuesObj[it.key()] = it.value();
        }
    }

    QJsonObject obj;
    obj["id"] = message.id;
    obj["values"] = valuesObj;
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleGetAllKeys(QWebSocket *client, const MessageRequest &message)
{
    bool ok;
    KeyValue kv = KeyValue::fromJson(message.data, &ok);
    if (!ok || !kv.isValid())
    {
        qWarning() << "Invalid get all keys message format from" << client->peerAddress().toString();
        client->close();
        return "";
    }

    auto collection = kv.col;
    auto database = m_databases[collection].get();
    QJsonArray keysArray;
    
    if (database == nullptr) {
        m_databases.erase(collection);
    } else {
        auto keys = database->getAllKeys();
        foreach (const QString &key, keys)
        {
            keysArray.append(key);
        }
    }

    QJsonObject obj;
    obj["id"] = message.id;
    obj["keys"] = keysArray;
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleConnections(QWebSocket *client, const MessageRequest &message)
{
    QJsonObject obj;
    obj["id"] = message.id;
    QJsonArray connectionsArray;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (QWebSocket *socket : m_clients)
    {
        if (!socket)
        {
            continue;
        }
        QJsonObject connectionObj;
        connectionObj["ip"] = socket->peerAddress().toString();
        QString connectionName;
        auto nameIt = m_clientNames.find(socket->objectName());
        if (nameIt != m_clientNames.end())
        {
            connectionName = nameIt->second;
        }
        qint64 connectedAt = now;
        auto it = m_connectionTimes.find(socket->objectName());
        if (it != m_connectionTimes.end())
        {
            connectedAt = it->second;
        }
        connectionObj["since"] = now - connectedAt;
        if (connectionName.isEmpty())
        {
            connectionObj["name"] = QJsonValue();
        }
        else
        {
            connectionObj["name"] = connectionName;
        }
        connectionObj["self"] = (socket == client);
        connectionsArray.append(connectionObj);
    }

    obj["connections"] = connectionsArray;
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

QString WebSocket::handleManageApiKey(QWebSocket *client, const MessageRequest &message)
{
    auto clientKeyIt = m_clientKeys.find(client->objectName());
    if (clientKeyIt == m_clientKeys.end() || clientKeyIt->second != m_masterKey)
    {
        QJsonObject response;
        response["id"] = message.id;
        response["error"] = "only the master key may manage API keys";
        QJsonDocument responseDoc(response);
        return responseDoc.toJson(QJsonDocument::Compact);
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.data.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning() << "Invalid manage api key message format from" << client->peerAddress().toString();
        rejectClient(client, QStringLiteral("Invalid manage api key message"));
        return "";
    }

    QJsonObject payload = doc.object();
    QString action = payload.value("action").toString().trimmed().toLower();

    QJsonObject response;
    response["id"] = message.id;

    if (action == "add")
    {
        const QString key = payload.value("key").toString();
        const QString scopeString = payload.value("scope").toString();
        ApiKeyScope scopeValue;
        if (!parseScope(scopeString, &scopeValue))
        {
            response["error"] = "invalid scope";
        }
        else
        {
            QString errorMessage;
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
        const QString key = payload.value("key").toString();
        QString errorMessage;
        if (removeApiKey(key, &errorMessage))
        {
            response["status"] = "ok";
        }
        else
        {
            response["error"] = errorMessage;
        }
    }
    else
    {
        response["error"] = "unknown action";
    }

    QJsonDocument responseDoc(response);
    return responseDoc.toJson(QJsonDocument::Compact);
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

WebSocket::RequiredPermission WebSocket::permissionForType(const QString &type) const
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

bool WebSocket::registerApiKey(const QString &key, ApiKeyScope scope, bool deletable, QString *errorMessage, bool persistToStorage)
{
    if (key.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = "api key cannot be empty";
        }
        return false;
    }

    ApiKeyScope scopeToStore = scope;
    bool deletableToStore = deletable;
    if (key == m_masterKey)
    {
        scopeToStore = ApiKeyScope::ReadWriteDelete;
        deletableToStore = false;
    }

    bool shouldPersist = false;
    auto it = m_apiKeys.find(key);
    if (it == m_apiKeys.end())
    {
        m_apiKeys.emplace(key, ApiKeyEntry{scopeToStore, deletableToStore});
        // Persist all keys except the master key
        shouldPersist = (key != m_masterKey);
    }
    else
    {
        it->second.scope = scopeToStore;
        if (!it->second.deletable)
        {
            it->second.deletable = false;
        }
        else
        {
            it->second.deletable = deletableToStore;
        }
        // Persist all keys except the master key
        shouldPersist = (key != m_masterKey);
    }

    const QList<QWebSocket *> currentClients = m_clients;
    for (QWebSocket *clientSocket : currentClients)
    {
        if (!clientSocket)
        {
            continue;
        }
        auto clientKeyIt = m_clientKeys.find(clientSocket->objectName());
        if (clientKeyIt != m_clientKeys.end() && clientKeyIt->second == key)
        {
            m_clientScopes[clientSocket->objectName()] = scopeToStore;
        }
    }
    
    if (shouldPersist && m_storage != nullptr && persistToStorage)
    {
        if (!m_storage->upsertApiKey(key, scopeToString(scopeToStore), deletableToStore))
        {
            qWarning() << "Failed to persist API key to SQLite";
        }
    }
    
    if (errorMessage)
    {
        errorMessage->clear();
    }
    return true;
}

bool WebSocket::removeApiKey(const QString &key, QString *errorMessage)
{
    auto it = m_apiKeys.find(key);
    if (it == m_apiKeys.end())
    {
        if (errorMessage)
        {
            *errorMessage = "api key not found";
        }
        return false;
    }

    if (!it->second.deletable)
    {
        if (errorMessage)
        {
            *errorMessage = "api key cannot be removed";
        }
        return false;
    }

    m_apiKeys.erase(it);

    const QList<QWebSocket *> currentClients = m_clients;
    for (QWebSocket *clientSocket : currentClients)
    {
        if (!clientSocket)
        {
            continue;
        }

        auto clientKeyIt = m_clientKeys.find(clientSocket->objectName());
        if (clientKeyIt != m_clientKeys.end() && clientKeyIt->second == key)
        {
            rejectClient(clientSocket, QStringLiteral("API key revoked"));
        }
    }

    if (m_storage != nullptr && key != m_masterKey)
    {
        if (!m_storage->deleteApiKey(key))
        {
            qWarning() << "Failed to delete API key from SQLite";
        }
    }

    if (errorMessage)
    {
        errorMessage->clear();
    }
    return true;
}

void WebSocket::loadApiKeysFromDisk()
{
    if (m_storage == nullptr)
    {
        qInfo() << "SQLite storage unavailable, API keys will not persist";
        return;
    }

    const auto rows = m_storage->fetchApiKeys();
    int loadedCount = 0;
    for (const auto &row : rows)
    {
        ApiKeyScope scope;
        if (!parseScope(row.scope, &scope))
        {
            qWarning() << "Invalid scope for persisted API key, skipping:" << row.key;
            continue;
        }

        if (row.key == m_masterKey)
        {
            // Ensure master key always registered with full scope regardless of stored value
            scope = ApiKeyScope::ReadWriteDelete;
        }

        QString errorMessage;
        if (registerApiKey(row.key, scope, row.deletable, &errorMessage, false))
        {
            ++loadedCount;
        }
        else
        {
            qWarning() << "Failed to load API key:" << row.key << errorMessage;
        }
    }
    qInfo() << "Loaded" << loadedCount << "API keys from SQLite";
}

bool WebSocket::parseScope(const QString &scopeString, ApiKeyScope *scopeOut) const
{
    if (scopeOut == nullptr)
    {
        return false;
    }

    QString normalized = scopeString;
    normalized = normalized.trimmed().toLower();
    normalized.remove(' ');
    normalized.remove(',');
    normalized.remove('-');
    normalized.remove('_');

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

QString WebSocket::scopeToString(ApiKeyScope scope) const
{
    switch (scope)
    {
    case ApiKeyScope::ReadOnly:
        return QStringLiteral("readonly");
    case ApiKeyScope::ReadWrite:
        return QStringLiteral("read_write");
    case ApiKeyScope::ReadWriteDelete:
        return QStringLiteral("read_write_delete");
    }
    return QStringLiteral("unknown");
}

WebSocket::ApiKeyEntry *WebSocket::lookupApiKey(const QString &key)
{
    auto it = m_apiKeys.find(key);
    if (it == m_apiKeys.end())
    {
        return nullptr;
    }
    return &it->second;
}

void WebSocket::rejectClient(QWebSocket *socket, const QString &reason)
{
    if (!socket)
    {
        return;
    }

    qWarning() << QTime::currentTime().toString() << "Closing client"
               << socket->peerAddress().toString() << "ID" << socket->objectName() << ":" << reason;

    m_clientScopes.erase(socket->objectName());
    m_clientKeys.erase(socket->objectName());
    m_clientNames.erase(socket->objectName());
    m_connectionTimes.erase(socket->objectName());
    m_clients.removeAll(socket);
    socket->close(QWebSocketProtocol::CloseCodePolicyViolated, reason.left(120));
    socket->deleteLater();
}

void WebSocket::socketDisconnected()
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (client)
    {
        qInfo() << QTime::currentTime().toString() << "Client disconnected:" 
                 << client->peerAddress().toString() << "ID" << client->objectName();
        m_clientScopes.erase(client->objectName());
        m_clientKeys.erase(client->objectName());
        m_clientNames.erase(client->objectName());
        m_connectionTimes.erase(client->objectName());
        m_clients.removeAll(client);
        client->deleteLater();
    }
}
