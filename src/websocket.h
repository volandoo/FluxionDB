#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <QObject>
#include <QString>
#include <QList>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QTimer>
#include <QStringList>
#include <unordered_map>
#include <memory>
#include <QDateTime>

#include "messagerequest.h"
#include "collection.h"
class SqliteStorage;

namespace MessageType {
    inline const QString Auth = QStringLiteral("auth");
    inline const QString Insert = QStringLiteral("ins");
    inline const QString QuerySessions = QStringLiteral("qry");
    inline const QString QueryCollections = QStringLiteral("cols");
    inline const QString QueryDocument = QStringLiteral("qdoc");
    inline const QString DeleteDocument = QStringLiteral("ddoc");
    inline const QString DeleteCollection = QStringLiteral("dcol");
    inline const QString DeleteRecord = QStringLiteral("drec");
    inline const QString DeleteMultipleRecords = QStringLiteral("dmrec");
    inline const QString DeleteRecordsRange = QStringLiteral("drrng");
    inline const QString SetValue = QStringLiteral("sval");
    inline const QString GetValue = QStringLiteral("gval");
    inline const QString GetValues = QStringLiteral("gvalues");
    inline const QString RemoveValue = QStringLiteral("rval");
    inline const QString GetAllValues = QStringLiteral("gvals");
    inline const QString GetAllKeys = QStringLiteral("gkeys");
    inline const QString ManageApiKey = QStringLiteral("keys");
    inline const QString Connections = QStringLiteral("conn");
}

// comment
class WebSocket : public QObject
{
    Q_OBJECT
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

    explicit WebSocket(const QString& masterKey, const QString& dataFolder, int flushIntervalSeconds = 15, QObject *parent = nullptr);
    ~WebSocket();

    void start(quint16 port = 8080);

private slots:
    void onNewConnection();
    void processMessage(const QString &message);
    void socketDisconnected();
    void flushToDisk();

private:
    void handleMessage(QWebSocket* client, const MessageRequest& message);
    
    QString handleQueryDocument(QWebSocket* client, const MessageRequest& message);
    QString handleQuerySessions(QWebSocket* client, const MessageRequest& message);
    QString handleQueryCollections(QWebSocket* client, const MessageRequest& message);
    QString handleDeleteDocument(QWebSocket* client, const MessageRequest& message);
    QString handleDeleteCollection(QWebSocket* client, const MessageRequest& message);
    QString handleDeleteRecord(QWebSocket* client, const MessageRequest& message);
    QString handleDeleteMultipleRecords(QWebSocket* client, const MessageRequest& message);
    QString handleDeleteRecordsRange(QWebSocket* client, const MessageRequest& message);
    QString handleInsert(QWebSocket* client, const MessageRequest& message);
    QString handleConnections(QWebSocket* client, const MessageRequest& message);

    // key value
    QString handleSetValue(QWebSocket* client, const MessageRequest& message);
    QString handleGetValue(QWebSocket* client, const MessageRequest& message);
    QString handleGetValues(QWebSocket* client, const MessageRequest& message);
    QString handleRemoveValue(QWebSocket* client, const MessageRequest& message);
    QString handleGetAllValues(QWebSocket* client, const MessageRequest& message);
    QString handleGetAllKeys(QWebSocket* client, const MessageRequest& message);
    QString handleManageApiKey(QWebSocket* client, const MessageRequest& message);

    bool hasPermission(ApiKeyScope scope, RequiredPermission required) const;
    RequiredPermission permissionForType(const QString& type) const;
    bool registerApiKey(const QString& key, ApiKeyScope scope, bool deletable, QString* errorMessage = nullptr, bool persistToStorage = true);
    bool removeApiKey(const QString& key, QString* errorMessage = nullptr);
    bool parseScope(const QString& scopeString, ApiKeyScope* scopeOut) const;
    QString scopeToString(ApiKeyScope scope) const;
    void loadApiKeysFromDisk();

    struct ApiKeyEntry {
        ApiKeyScope scope;
        bool deletable;
    };

    ApiKeyEntry* lookupApiKey(const QString& key);

    void rejectClient(QWebSocket* socket, const QString& reason);

    QWebSocketServer *m_server;
    QList<QWebSocket *> m_clients;
    
    // Configuration
    QString m_masterKey;
    QString m_dataFolder;

    // In-memory databases
    std::unique_ptr<SqliteStorage> m_storage;
    std::unordered_map<QString, std::unique_ptr<Collection>> m_databases;
    std::unordered_map<QString, ApiKeyScope> m_clientScopes;
    std::unordered_map<QString, ApiKeyEntry> m_apiKeys;
    std::unordered_map<QString, QString> m_clientKeys;
    std::unordered_map<QString, QString> m_clientNames;
    std::unordered_map<QString, qint64> m_connectionTimes;

    // flush timer
    QTimer m_flushTimer;
};

#endif // WEBSOCKET_H 
