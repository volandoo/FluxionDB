QT -= gui
QT += core websockets sql

CONFIG += c++17 console
CONFIG -= app_bundle

SOURCES += \
    src/insertrequest.cpp \
    src/datarecord.cpp \
    src/main.cpp \
    src/messagerequest.cpp \
    src/deletecollection.cpp \
    src/querysessions.cpp \
    src/querydocument.cpp \
    src/websocket.cpp \
    src/collection.cpp \
    src/deletedocument.cpp \
    src/keyvalue.cpp \
    src/deleterecord.cpp \
    src/deletemultiplerecords.cpp \
    src/deleterecordsrange.cpp \
    src/sqlitestorage.cpp

HEADERS += \
    src/insertrequest.h \
    src/datarecord.h \
    src/datarecordheader.h \
    src/message.h \
    src/messagerequest.h \
    src/deletecollection.h \
    src/querysessions.h \
    src/querydocument.h \
    src/websocket.h \
    src/collection.h \
    src/deletedocument.h \
    src/keyvalue.h \
    src/deleterecord.h \
    src/deletemultiplerecords.h \
    src/deleterecordsrange.h \
    src/sqlitestorage.h
