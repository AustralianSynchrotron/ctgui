cmake_minimum_required(VERSION 3.9)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project(ctgui VERSION 2.0 )
add_definitions(-DAPP_VERSION=${PROJECT_VERSION})
set(CMAKE_C_STANDARD "17")
set(CMAKE_CXX_STANDARD "17")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 COMPONENTS Widgets REQUIRED)
include_directories(${Qt5Widgets_INCLUDE_DIRS})

find_path(POPTMX_INC poptmx.h)
if(NOT POPTMX_INC)
  message(FATAL_ERROR ": Can't find folder containing poptmx.h.")
endif()
include_directories(${POPTMX_INC})


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

add_executable(ctgui${EXESUFFIX}
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
    indicators.hpp
    ctgui.qrc
)

target_compile_options(ctgui${EXESUFFIX}
  PRIVATE -fPIC
)

target_link_libraries(ctgui${EXESUFFIX}
  qtpv
  qtpvwidgets
  qcamotor
  qcamotorgui
  poptmx
  Qt5::Widgets
)


set(DESKTOPFILE "CTgui.desktop")
configure_file(${DESKTOPFILE}.in ${DESKTOPFILE} @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DESKTOPFILE}
    DESTINATION share/applications
)


install(TARGETS ctgui${EXESUFFIX}
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


