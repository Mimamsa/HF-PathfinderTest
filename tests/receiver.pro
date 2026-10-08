QT += widgets serialport testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = receiver-test
INCLUDEPATH += ..
SOURCES += receiver_test.cpp ../PathfinderReceiver.cpp
HEADERS += ../PathfinderReceiver.h
unix:!macx: LIBS += -lutil
