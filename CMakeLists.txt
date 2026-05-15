cmake_minimum_required(VERSION 3.22)

project(fluxiondb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

set(UWEBSOCKETS_ROOT "" CACHE PATH "Path to the uWebSockets source checkout")
if(NOT UWEBSOCKETS_ROOT AND DEFINED ENV{UWEBSOCKETS_ROOT})
    set(UWEBSOCKETS_ROOT "$ENV{UWEBSOCKETS_ROOT}")
endif()

find_path(UWEBSOCKETS_INCLUDE_DIR
    NAMES App.h
    PATHS
        "${UWEBSOCKETS_ROOT}/src"
        "/usr/local/include/uWebSockets"
        "/usr/local/include"
)

find_path(USOCKETS_INCLUDE_DIR
    NAMES libusockets.h
    PATHS
        "${UWEBSOCKETS_ROOT}/uSockets/src"
        "/usr/local/include"
)

if(EXISTS "${UWEBSOCKETS_ROOT}/uSockets/uSockets.a")
    set(USOCKETS_LIBRARY "${UWEBSOCKETS_ROOT}/uSockets/uSockets.a")
else()
    find_library(USOCKETS_LIBRARY
        NAMES uSockets libuSockets
        PATHS
            "${UWEBSOCKETS_ROOT}/uSockets"
            "/usr/local/lib"
    )
endif()

if(NOT UWEBSOCKETS_INCLUDE_DIR OR NOT USOCKETS_INCLUDE_DIR OR NOT USOCKETS_LIBRARY)
    message(FATAL_ERROR "uWebSockets/uSockets not found. Set -DUWEBSOCKETS_ROOT=/path/to/uWebSockets after building uSockets.")
endif()

add_executable(fluxiondb
    src/collection.cpp
    src/datarecord.cpp
    src/deletecollection.cpp
    src/deletedocument.cpp
    src/deletemultiplerecords.cpp
    src/deleterecord.cpp
    src/deleterecordsrange.cpp
    src/insertrequest.cpp
    src/keyvalue.cpp
    src/main.cpp
    src/messagerequest.cpp
    src/querydocument.cpp
    src/querysessions.cpp
    src/querysessionsresponse.cpp
    src/sqlitestorage.cpp
    src/websocket.cpp
)

target_include_directories(fluxiondb PRIVATE
    src
    "${UWEBSOCKETS_INCLUDE_DIR}"
    "${USOCKETS_INCLUDE_DIR}"
)

target_compile_options(fluxiondb PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(fluxiondb PRIVATE
    SQLite::SQLite3
    nlohmann_json::nlohmann_json
    "${USOCKETS_LIBRARY}"
    ZLIB::ZLIB
    Threads::Threads
)
