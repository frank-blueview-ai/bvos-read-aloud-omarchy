QT += quick quickcontrols2 multimedia dbus
CONFIG += c++17
TARGET = omarchy-read-aloud
SOURCES += src/passage.cpp src/main.cpp src/speechplayback.cpp
HEADERS += src/passage.h src/speechplayback.h
RESOURCES += src/resources.qrc
