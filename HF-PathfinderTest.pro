QT += core gui widgets serialport
TEMPLATE = app
TARGET = HF-PathfinderTest
CONFIG += c++17

lessThan(QT_MAJOR_VERSION, 5): error("Qt 5.15 or Qt 6.2+ is required")
equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 15): error("Qt 5.15 or newer is required")
equals(QT_MAJOR_VERSION, 6):lessThan(QT_MINOR_VERSION, 2): error("Qt 6.2 or newer is required")
!qtHaveModule(serialport): error("Qt SerialPort development files are missing for this qmake. Install the matching Qt SerialPort module; see README.md.")

SOURCES += main.cpp            PathfinderReceiver.cpp

HEADERS += PathfinderReceiver.h

CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT
