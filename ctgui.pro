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

LIBS += -lqcamotorgui

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
