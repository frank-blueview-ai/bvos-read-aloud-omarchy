QT += quick quickcontrols2 multimedia
CONFIG += c++17
standalone {
    DEFINES += BVOS_STANDALONE
    TARGET = bvos-read-aloud
} else {
    QT += dbus
    TARGET = omarchy-read-aloud
}
SOURCES += src/passage.cpp src/main.cpp src/speechplayback.cpp
HEADERS += src/passage.h src/speechplayback.h
RESOURCES += src/resources.qrc
