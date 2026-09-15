cmake_minimum_required(VERSION 3.16)
project(multipaste VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Test)

# The KDE global shortcut (KGlobalAccel) is optional: it requires KDE Frameworks
# 6 (Ubuntu 24.10+, KDE neon). Without it the app still builds and works - the
# X11 "Alt+Right" trigger remains fully functional, and on Wayland the shortcut
# is just skipped (tray / activity panel / DBus / widget all still work).
find_package(KF6GlobalAccel QUIET)

find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
pkg_check_modules(XTST REQUIRED IMPORTED_TARGET xtst)
find_package(Threads REQUIRED)

add_executable(multipaste
    src/main.cpp
    src/Settings.h
    src/Settings.cpp
    src/HistoryItem.h
    src/HistoryItem.cpp
    src/PasteLifter.h
    src/PasteLifter.cpp
    src/GlobalShortcutTrigger.h
    src/GlobalShortcutTrigger.cpp
    src/X11MouseTrigger.h
    src/X11MouseTrigger.cpp
    src/MultipasteCore.h
    src/MultipasteCore.cpp
    src/ExternalClipboard.h
    src/WaylandClipboard.h
    src/WaylandClipboard.cpp
    src/KlipperClipboard.h
    src/KlipperClipboard.cpp
    src/TrayIcon.h
    src/TrayIcon.cpp
    src/SettingsDialog.h
    src/SettingsDialog.cpp
    src/ActivityPanel.h
    src/ActivityPanel.cpp
    src/MultipasteDbusServer.h
    src/MultipasteDbusServer.cpp
)

target_include_directories(multipaste PRIVATE src)
target_link_libraries(multipaste PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    PkgConfig::X11
    PkgConfig::XTST
    Threads::Threads
)
if(KF6GlobalAccel_FOUND)
    target_link_libraries(multipaste PRIVATE KF6::GlobalAccel)
    target_compile_definitions(multipaste PRIVATE MULTIPASTE_HAS_KGLOBALACCEL)
endif()

include(GNUInstallDirs)
install(TARGETS multipaste RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/multipaste.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# System-wide install of the Plasma 6 widget (per-user alternative:
# scripts/install-widget.sh -> ~/.local/share/plasma/plasmoids).
install(FILES plasmoid/metadata.json
        DESTINATION ${CMAKE_INSTALL_DATADIR}/plasma/plasmoids/org.multipaste.activity)
install(FILES plasmoid/contents/ui/main.qml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/plasma/plasmoids/org.multipaste.activity/contents/ui)
install(FILES plasmoid/contents/ui/main.plasma5.qml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/plasma/plasmoids/org.multipaste.activity/contents/ui)
install(PROGRAMS plasmoid/contents/code/status.sh
                     plasmoid/contents/code/paste.sh
                     plasmoid/contents/code/newseq.sh
                     plasmoid/contents/code/clear.sh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/plasma/plasmoids/org.multipaste.activity/contents/code)

include(CTest)
if(BUILD_TESTING)
    add_executable(tst_history tests/tst_history.cpp src/HistoryItem.cpp)
    target_include_directories(tst_history PRIVATE src)
    target_link_libraries(tst_history PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)

    add_executable(tst_core tests/tst_core.cpp
        src/HistoryItem.cpp
        src/MultipasteCore.cpp
        src/PasteLifter.cpp
        src/ExternalClipboard.h
        src/WaylandClipboard.cpp
        src/KlipperClipboard.cpp)
    target_include_directories(tst_core PRIVATE src)
    target_link_libraries(tst_core PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::DBus PkgConfig::X11
    PkgConfig::XTST)

    add_test(NAME tst_history COMMAND tst_history)
    add_test(NAME tst_core COMMAND tst_core)
    set_tests_properties(tst_history tst_core PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()