#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QStringList>
#include <QFile>
#include <QDateTime>
#include <exception>
#include "websocket.h"

void logException(const QString& message) {
    QFile logFile("crash.log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream stream(&logFile);
        stream << QDateTime::currentDateTime().toString() << ": " << message << "\n";
        logFile.close();
    }
    qCritical() << "CRASH:" << message;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("start");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Start the server with specific arguments");

    // Add the help and version options automatically
    parser.addHelpOption();
    parser.addVersionOption();

    // Define options
    QCommandLineOption secretKeyOption(
        QStringList() << "s" << "secret-key",
        "The secret key for the server",
        "secret-key"
    );
    
    QCommandLineOption dataFolderOption(
        QStringList() << "d" << "data",
        "The folder to store persistent data (if not specified, data won't be persisted)",
        "data"
    );
    
    QCommandLineOption flushIntervalOption(
        QStringList() << "f" << "flush-interval",
        "The interval in seconds to flush data to disk (default: 15)",
        "seconds",
        "15"
    );
    
    // Add options to parser
    parser.addOption(secretKeyOption);
    parser.addOption(dataFolderOption);
    parser.addOption(flushIntervalOption);

    // Process the command line arguments
    parser.process(app);

    // Get values
    QString secretKey = parser.value(secretKeyOption);
    QString dataFolder = parser.value(dataFolderOption);
    int flushInterval = parser.value(flushIntervalOption).toInt();

    // Validate required options
    if (secretKey.isEmpty()) {
        qFatal("secret-key is not set");
    }
    
    qInfo() << "Server started";
    // Create and start WebSocket server
    WebSocket server(secretKey, dataFolder, flushInterval);
    server.start(8080);

    return app.exec();
} 
