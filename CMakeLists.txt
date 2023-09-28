cmake_minimum_required(VERSION 3.0)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project(ctgui
        VERSION 1.0 )
add_definitions(-DAPP_VERSION=${PROJECT_VERSION})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 COMPONENTS Widgets REQUIRED)
include_directories(${Qt5Widgets_INCLUDE_DIRS})

find_path(QEPICSPV_INC qtpv.h)
if(NOT QEPICSPV_INC)
  message(FATAL_ERROR ": Can't find folder containing qtpv.h")
endif()
include_directories(${QEPICSPV_INC})

find_path(QCAMOTOR_INC qcamotorgui.h)
if(NOT QCAMOTOR_INC)
  message(FATAL_ERROR ": Can't find folder containing qcamotorgui.h")
endif()
include_directories(${QEPICSPV_INC})

set(CMAKE_EXECUTABLE_SUFFIX "_exp") # exerimental version

add_executable(ctgui
    main.cpp
    ctgui_mainwindow.h
    ctgui_mainwindow.cpp
    ctgui_mainwindow.ui
    additional_classes.h
    additional_classes.cpp
    detector.h
    detector.cpp
    triggct.h
    triggct.cpp
    shutter.h
    shutter.cpp
    shutter.ui
    ushutterconf.ui
    positionlist.h
    positionlist.cpp
    positionlist.ui
    script.ui
    upvorcom.ui
    ctgui.qrc
)

target_compile_options(ctgui
  PRIVATE -fPIC
)

target_link_libraries(ctgui
  qtpv
  qtpvwidgets
  qcamotor
  qcamotorgui
  Qt5::Widgets
)


set(DESKTOPFILE "CTgui.desktop")
configure_file(${DESKTOPFILE}.in ${DESKTOPFILE} @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DESKTOPFILE}
    DESTINATION share/applications
)


install(TARGETS ctgui
    DESTINATION bin
)


install(FILES listOfKnownShutters.ini
    DESTINATION share
)

install(FILES CTguiIcon.png
    DESTINATION share/icons
)


install(PROGRAMS libexec/ctgui.log.sh
    DESTINATION libexec
)


