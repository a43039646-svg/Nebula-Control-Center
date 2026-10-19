# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.21)
project(NebulaControlCenter VERSION 2.1.0 LANGUAGES C)

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

add_executable(nebula-control-center
    src/main.c
    src/system_info.c
    src/process_manager.c
    src/storage.c
    src/network.c
    src/services.c
    src/i18n.c
    src/doctor.c
    src/startup.c
    src/gaming.c
    src/permissions.c
)
target_link_libraries(nebula-control-center PRIVATE PkgConfig::GTK4 m)
target_compile_definitions(nebula-control-center PRIVATE
    _GNU_SOURCE
    NEBULA_LOCALEDIR="${CMAKE_INSTALL_FULL_DATADIR}/nebula-control-center/locales"
)
target_compile_options(nebula-control-center PRIVATE -Wall -Wextra)

install(TARGETS nebula-control-center RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES org.nebula.ControlCenter.desktop DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
foreach(icon_size 16 24 32 48 64 128 256 512)
    install(FILES "icons/nebula-control-center-${icon_size}.png"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${icon_size}x${icon_size}/apps"
            RENAME nebula-control-center.png)
endforeach()
install(DIRECTORY locales/ DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/nebula-control-center/locales"
        FILES_MATCHING PATTERN "*.lang")
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/nebula-control-center")

set(CPACK_GENERATOR TGZ)
find_program(NEBULA_DPKG_DEB dpkg-deb)
if(NEBULA_DPKG_DEB)
    list(APPEND CPACK_GENERATOR DEB)
endif()
find_program(NEBULA_RPMBUILD rpmbuild)
if(NEBULA_RPMBUILD)
    list(APPEND CPACK_GENERATOR RPM)
endif()
set(CPACK_PACKAGE_NAME nebula-control-center)
set(CPACK_PACKAGE_VENDOR "Nebula Project")
set(CPACK_PACKAGE_CONTACT "Nebula Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Linux system monitor and control center")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Nebula Project")
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
find_program(NEBULA_DPKG_SHLIBDEPS dpkg-shlibdeps)
if(NEBULA_DPKG_SHLIBDEPS)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
include(CPack)
