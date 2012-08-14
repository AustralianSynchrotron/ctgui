#-------------------------------------------------
#
# Project created by QtCreator 2010-10-26T15:35:20
#
#-------------------------------------------------

QT       += core gui

TARGET = ctgui
TEMPLATE = app


SOURCES += main.cpp\
        ctgui_mainwindow.cpp

IMBLEXEC   = $$system(command -v imblgui)
IMBLORIGIN = $$dirname(IMBLEXEC)
IMBLRPATH = $$system(objdump -x $${IMBLEXEC} \
                     | grep RPATH \
                     | sed -e \"s/RPATH//g\" \
                           -e \"s/^ *//g\" \
                           -e \"s \\\$ORIGIN $${IMBLORIGIN} g\" \
                     | cut -d':' -f1 )

QMAKE_LFLAGS += -Wl,-rpath,$$IMBLRPATH



LIBS += -lqcamotor -lqcamotorgui \
            -L$$IMBLRPATH \
            -lshutter1A

HEADERS  += ctgui_mainwindow.h \
    additional_classes.h

FORMS    += ctgui_mainwindow.ui \
    ctgui_variables.ui

RESOURCES += \
    ctgui.qrc

#OTHER_FILES +=

target.files = $$[TARGET]
target.path = $$[INSTALLBASE]/bin
INSTALLS += target
