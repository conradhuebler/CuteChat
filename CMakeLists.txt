# CuteChat - an LLM tool-calling agent core and a chat dock for Qt6 applications.
# Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>   (GPL-3.0-or-later)
# Claude Generated 2026
#
# Targets:
#   cutechat_tools (CuteChat::tools)     Qt Core only: tool contract, registry, dispatcher
#   cutechat_core  (CuteChat::core)      + Network: client, config, presets, session, chat log
#   cutechat       (CuteChat::cutechat)  + Widgets: ChatDock, EndpointDialog (CUTECHAT_BUILD_WIDGETS)
cmake_minimum_required(VERSION 3.20)
project(CuteChat VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CUTECHAT_TOPLEVEL ON)
else()
    set(CUTECHAT_TOPLEVEL OFF)
endif()

option(CUTECHAT_BUILD_WIDGETS "Build the chat dock and the endpoint dialog (needs Qt6::Widgets)" ON)
option(CUTECHAT_BUILD_TESTS "Build the tests (QtTest, no network)" ${CUTECHAT_TOPLEVEL})
option(CUTECHAT_INSTALL "Install headers, libraries and the CMake package" ${CUTECHAT_TOPLEVEL})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(_cutechat_qt_components Core Network)
if(CUTECHAT_BUILD_WIDGETS)
    list(APPEND _cutechat_qt_components Widgets)
endif()
if(CUTECHAT_BUILD_TESTS)
    list(APPEND _cutechat_qt_components Test)
endif()
find_package(Qt6 REQUIRED COMPONENTS ${_cutechat_qt_components})

# ---- tool layer: Qt Core only (an application's script dock or command palette may use it without a network) ----
add_library(cutechat_tools STATIC
    src/tool.cpp
    src/toolregistry.cpp
    src/tooldispatcher.cpp
    src/chathelp.cpp
    # headers listed so AUTOMOC finds the Q_OBJECT classes (they live in include/, not next to the .cpp)
    include/cutechat/tool.h
    include/cutechat/toolregistry.h
    include/cutechat/tooldispatcher.h
    include/cutechat/chathelp.h)
add_library(CuteChat::tools ALIAS cutechat_tools)
target_include_directories(cutechat_tools PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(cutechat_tools PUBLIC Qt6::Core)
target_compile_features(cutechat_tools PUBLIC cxx_std_17)
set_target_properties(cutechat_tools PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)

# ---- core: client, config, session, chat log ----
add_library(cutechat_core STATIC
    src/llmconfig.cpp
    src/llmpresets.cpp
    src/llmclient.cpp
    src/llmsession.cpp
    src/chatengine.cpp
    src/sessionkeys.cpp
    src/chatlog.cpp
    src/chatrecorder.cpp
    src/chatbackend.cpp
    include/cutechat/llmconfig.h
    include/cutechat/llmpresets.h
    include/cutechat/llmclient.h
    include/cutechat/llmsession.h
    include/cutechat/chatengine.h
    include/cutechat/sessionkeys.h
    include/cutechat/chatlog.h
    include/cutechat/chatrecorder.h
    include/cutechat/chatbackend.h)
add_library(CuteChat::core ALIAS cutechat_core)
target_link_libraries(cutechat_core PUBLIC cutechat_tools Qt6::Network)
set_target_properties(cutechat_core PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)

# ---- widgets ----
if(CUTECHAT_BUILD_WIDGETS)
    add_library(cutechat STATIC
        src/widgets/chatdock.cpp
        src/widgets/endpointdialog.cpp
        src/widgets/flowlayout.cpp
        src/widgets/chathost.cpp
        include/cutechat/chatdock.h
        include/cutechat/endpointdialog.h
        include/cutechat/chathost.h
        src/widgets/flowlayout.h)
    add_library(CuteChat::cutechat ALIAS cutechat)
    target_include_directories(cutechat PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    target_link_libraries(cutechat PUBLIC cutechat_core Qt6::Widgets)
    set_target_properties(cutechat PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
endif()

# ---- tests ----
if(CUTECHAT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- install ----
if(CUTECHAT_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    set(_targets cutechat_tools cutechat_core)
    if(CUTECHAT_BUILD_WIDGETS)
        list(APPEND _targets cutechat)
    endif()
    install(TARGETS ${_targets} EXPORT CuteChatTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY include/cutechat DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT CuteChatTargets NAMESPACE CuteChat:: FILE CuteChatTargets.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CuteChat)
    configure_package_config_file(cmake/CuteChatConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/CuteChatConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CuteChat)
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/CuteChatConfigVersion.cmake
        VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CuteChatConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/CuteChatConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CuteChat)
endif()
