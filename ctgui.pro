# -------------------------------------------------
# Project created by QtCreator 2010-10-26T15:35:20
# -------------------------------------------------
QT += core \
    gui
TARGET = ctgui$$SUFFIX
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

#IMBLEXEC = $$system(command -v imblgui)
#IMBLORIGIN = $$dirname(IMBLEXEC)
#IMBLRPATH = $$system(objdump -x $${IMBLEXEC} | grep RPATH | sed -e \"s/RPATH//g\" -e \"s/^ *//g\" -e \"s \\\$ORIGIN $${IMBLORIGIN} g\" | cut -d':' -f1 )
#QMAKE_LFLAGS += -Wl,-rpath,$$IMBLRPATH
LIBS += -lqtpv \
    -lqtpvwidgets \
    -lqcamotor \
    -lqcamotorgui
#    -L$$IMBLRPATH \
#    -lshutter1A \
#    -lcomponent
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
## - warn dialog on existing image
## - adjust step-and-shot names
## - make serial test aquisition
## - rework tiles with new lists
## - add new detectors
## - selection between scan speed / period / fly ration
