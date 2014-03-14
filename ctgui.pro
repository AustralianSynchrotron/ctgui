#-------------------------------------------------
#
# Project created by QtCreator 2010-10-26T15:35:20
#
#-------------------------------------------------

QT       += core gui

TARGET = ctgui$$SUFFIX
TARGET.path = $$PREFIX/
TEMPLATE = app


SOURCES += main.cpp\
        ctgui_mainwindow.cpp \
    additional_classes.cpp \
    detector.cpp \
    triggct.cpp

IMBLEXEC   = $$system(command -v imblgui)
IMBLORIGIN = $$dirname(IMBLEXEC)
IMBLRPATH = $$system(objdump -x $${IMBLEXEC} \
                     | grep RPATH \
                     | sed -e \"s/RPATH//g\" \
                           -e \"s/^ *//g\" \
                           -e \"s \\\$ORIGIN $${IMBLORIGIN} g\" \
                     | cut -d':' -f1 )

QMAKE_LFLAGS += -Wl,-rpath,$$IMBLRPATH



LIBS +=     -lqtpv -lqtpvwidgets \
            -lqcamotor -lqcamotorgui \
            -L$$IMBLRPATH \
            -lshutter1A -lcomponent

HEADERS  += ctgui_mainwindow.h \
    additional_classes.h \
    detector.h \
    triggct.h

FORMS    += ctgui_mainwindow.ui \
    script.ui

RESOURCES += \
    ctgui.qrc

#OTHER_FILES +=

target.files = $$[TARGET]
target.path = $$[INSTALLBASE]/bin
INSTALLS += target
