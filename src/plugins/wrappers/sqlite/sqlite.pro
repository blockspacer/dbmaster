TEMPLATE=lib
CONFIG+=release plugin
QT = core gui sql
VERSION=0.8
INCLUDEPATH+=../../../src/plugins
TARGET=sqlitewrapper
HEADERS += \
    sqlitewrapper.h

SOURCES += \
    sqlitewrapper.cpp

# ##
# MS Windows
win32: {
    isEmpty(PREFIX):PREFIX = ..\\..\\..\\src\\install
    DEFINES += PREFIX=\\\"$${PREFIX}\\\"
    target.path = $${PREFIX}\\plugins
    INSTALLS = target
}


# ##
# All unix-like
unix:!macx {
    isEmpty( PREFIX ):PREFIX = /usr
    DEFINES += PREFIX=\\\"$${PREFIX}\\\"
    target.path = $${PREFIX}/share/dbmaster/plugins
    INSTALLS = target
}

