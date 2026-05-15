#include <exception>
#include <fstream>
#include <iostream>
#include <string>

#include "websocket.h"

namespace {

void logException(const std::string& message)
{
    std::ofstream logFile("crash.log", std::ios::app);
    if (logFile)
    {
        logFile << message << '\n';
    }
    std::cerr << "CRASH: " << message << '\n';
}

std::string optionValue(int argc, char* argv[], std::string_view shortName, std::string_view longName, std::string defaultValue = {})
{
    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg(argv[i]);
        const std::string longPrefix = std::string(longName) + "=";
        const std::string shortPrefix = std::string(shortName) + "=";
        if (arg == longName || arg == shortName)
        {
            if (i + 1 < argc)
            {
                return argv[i + 1];
            }
            return {};
        }
        if (arg.rfind(longPrefix, 0) == 0)
        {
            return std::string(arg.substr(longPrefix.size()));
        }
        if (arg.rfind(shortPrefix, 0) == 0)
        {
            return std::string(arg.substr(shortPrefix.size()));
        }
    }
    return defaultValue;
}

void printHelp(const char* executable)
{
    std::cout << "Usage: " << executable << " --secret-key <key> [--data <dir>] [--flush-interval <seconds>] [--port <port>]\n";
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string_view arg(argv[i]);
            if (arg == "--help" || arg == "-h")
            {
                printHelp(argv[0]);
                return 0;
            }
            if (arg == "--version" || arg == "-v")
            {
                std::cout << "FluxionDB 1.0\n";
                return 0;
            }
        }

        const std::string secretKey = optionValue(argc, argv, "-s", "--secret-key");
        const std::string dataFolder = optionValue(argc, argv, "-d", "--data");
        const std::string flushIntervalText = optionValue(argc, argv, "-f", "--flush-interval", "15");
        const std::string portText = optionValue(argc, argv, "-p", "--port", "8080");

        if (secretKey.empty())
        {
            std::cerr << "secret-key is not set\n";
            return 2;
        }

        int flushInterval = 15;
        int port = 8080;
        try
        {
            flushInterval = std::stoi(flushIntervalText);
            port = std::stoi(portText);
        }
        catch (const std::exception&)
        {
            std::cerr << "invalid numeric option\n";
            return 2;
        }

        std::cerr << "Server started\n";
        WebSocket server(secretKey, dataFolder, flushInterval);
        server.start(static_cast<std::uint16_t>(port));
        return 0;
    }
    catch (const std::exception& error)
    {
        logException(error.what());
        return 1;
    }
}
