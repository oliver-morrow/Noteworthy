cmake_minimum_required(VERSION 3.16)

project(noteworthy-qt VERSION 0.1 LANGUAGES CXX)

add_definitions(-DNOTEWORTHY_QT)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
    URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.12.0
    URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(Qt6 6.8 REQUIRED COMPONENTS Network Widgets Svg SvgWidgets WebSockets)

set(NW_HTTP_URL "http://localhost:8080" CACHE STRING "Noteworthy API origin")
set(NW_WS_URL "ws://localhost:8081" CACHE STRING "Noteworthy WebSocket origin")
option(NW_ENABLE_PDF "Show PDF import in the client" OFF)

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

qt_add_executable(noteworthy-qt
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    tabletgraphicsview.hpp tabletgraphicsview.cpp
    Stroke.hpp
    RoomState.hpp RoomState.cpp
    RandomIdGenerator.hpp RandomIdGenerator.cpp
    ClientWebSocketHandler.hpp clientwebsockethandler.cpp
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

target_include_directories(noteworthy-qt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(noteworthy-qt PRIVATE
    NW_HTTP_URL="${NW_HTTP_URL}"
    NW_WS_URL="${NW_WS_URL}"
    NW_ENABLE_PDF=$<BOOL:${NW_ENABLE_PDF}>
)
target_link_libraries(noteworthy-qt PRIVATE
    Qt6::Network Qt6::Widgets Qt6::WebSockets Qt6::Svg Qt6::SvgWidgets
    nlohmann_json::nlohmann_json
)

set_target_properties(noteworthy-qt PROPERTIES
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

qt_finalize_executable(noteworthy-qt)
