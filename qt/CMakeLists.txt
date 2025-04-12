cmake_minimum_required(VERSION 3.16)

project(noteworthy-qt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -g")

add_definitions(-DNOTEWORTHY_QT)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(ExternalProject)

set(EXTERNAL_INSTALL_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/external)

ExternalProject_Add(NLOHMANN
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION} -DJSON_BuildTests=OFF
)

include_directories(${CMAKE_SOURCE_DIR}) # probably unnecessary
include_directories(${EXTERNAL_INSTALL_LOCATION}/include)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

find_package(Qt6 REQUIRED COMPONENTS SvgWidgets)
find_package(Qt6 REQUIRED COMPONENTS WebSockets)
find_package(Qt6 REQUIRED COMPONENTS Svg)

set(PROJECT_SOURCES
        main.cpp
        loadingroompage.cpp
        loadingroompage.h  
        loadingroompage.ui
        # hannah stuff
        newuser.cpp
        newuser.h
        newuser.ui
        widget.cpp
        widget.h
        widget.ui
        drawingroom.cpp
        drawingroom.h
        drawingroom.ui
        Symbol.hpp
        hosts.hpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(noteworthy-qt
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        tabletgraphicsview.hpp tabletgraphicsview.cpp
        Stroke.hpp
        RoomState.hpp RoomState.cpp
        RandomIdGenerator.hpp RandomIdGenerator.cpp
        ClientWebSocketHandler.hpp ClientWebSocketHandler.cpp
        CanvasObject.hpp
        SendableObject.hpp
        AppNavigator.hpp AppNavigator.cpp
        EventTypeEnums.hpp
        Selection.hpp
        resource.qrc
        TouchState.hpp
        User.hpp
        SendableObject.hpp
        clickablegraphicsview.h
        Symbol.hpp
        Symbol.cpp
        Shape.hpp
        Shape.cpp
        load_file.hpp

    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET noteworthy-qt APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(noteworthy-qt SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(noteworthy-qt
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(noteworthy-qt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(noteworthy-qt PRIVATE Qt${QT_VERSION_MAJOR}::WebSockets)
target_link_libraries(noteworthy-qt PRIVATE Qt6::Svg)
target_link_libraries(noteworthy-qt PRIVATE Qt6::SvgWidgets)

add_dependencies(${PROJECT_NAME} NLOHMANN)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.noteworthy-qt)
endif()
set_target_properties(noteworthy-qt PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS noteworthy-qt
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(noteworthy-qt)
endif()
