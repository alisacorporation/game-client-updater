cmake_minimum_required(VERSION 3.19)
project(Lineage_2_Updater LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)

qt_standard_project_setup()

set(CMAKE_AUTORCC ON)

qt_add_executable(Lineage_2_Updater
    WIN32 MACOSX_BUNDLE
    main.cpp
    updater.cpp
    updater.h
    updater.ui
    res.qrc
)

target_link_libraries(Lineage_2_Updater
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
)

include(GNUInstallDirs)

install(TARGETS Lineage_2_Updater
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Lineage_2_Updater
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
