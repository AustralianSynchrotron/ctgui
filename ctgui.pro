# -------------------------------------------------
# Project created by QtCreator 2010-10-26T15:35:20
# -------------------------------------------------

VERSION = 2.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

QT += core \
      gui

TARGET = ctgui
TARGET.path = $$PREFIX/
TEMPLATE = app
SOURCES += main.cpp \
    ctgui_mainwindow.cpp \
    additional_classes.cpp \
    detector.cpp \
    triggct.cpp \
    shutter.cpp \
    positionlist.cpp
HEADERS += ctgui_mainwindow.h \
    additional_classes.h \
    detector.h \
    triggct.h \
    shutter.h \
    positionlist.h
FORMS += ctgui_mainwindow.ui \
    script.ui \
    shutter.ui \
    ushutterconf.ui \
    upvorcom.ui \
    positionlist.ui

LIBS += -lqtpv \
    -lqtpvwidgets \
    -lqcamotor \
    -lqcamotorgui
RESOURCES += ctgui.qrc

OTHER_FILES += listOfKnownShutters.ini
config.files = $$OTHER_FILES
config.path = /../../../../../../../../../../../etc
# This is how I force it to be installed in /etc. I know it is a dirty trick
INSTALLS += config

target.files = $$[TARGET]
target.path = $$[INSTALLBASE]/bin
INSTALLS += target

script.files = libexec/ctgui.log.sh
script.path = $$[INSTALLBASE]/libexec
INSTALLS += script
OTHER_FILES += libexec/ctgui.log.sh


## TODO ##
