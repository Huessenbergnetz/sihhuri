# SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(sihhuri
  main.cpp
  abstractbackup.h
  abstractbackup.cpp
  directorybackup.h
  directorybackup.cpp
  dbbackup.h
  dbbackup.cpp
  wordpressbackup.h
  wordpressbackup.cpp
  nextcloudbackup.h
  nextcloudbackup.cpp
  joomlabackup.h
  joomlabackup.cpp
  matomobackup.h
  matomobackup.cpp
  cyrusbackup.h
  cyrusbackup.cpp
  roundcubebackup.h
  roundcubebackup.cpp
  giteabackup.h
  giteabackup.cpp
  backupmanager.h
  backupmanager.cpp
)

target_compile_features(sihhuri PRIVATE cxx_std_14)

target_link_libraries(sihhuri
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        # SimpleMail::Core
        ${SYSTEMD_LIBRARIES}
)

target_include_directories(sihhuri
    PRIVATE
        ${SYSTEMD_INCLUDE_DIRS}
)

target_compile_definitions(sihhuri
    PRIVATE
        QT_NO_CAST_TO_ASCII
        QT_NO_CAST_FROM_ASCII
        QT_STRICT_ITERATORS
        QT_NO_URL_CAST_FROM_STRING
        QT_NO_CAST_FROM_BYTEARRAY
        QT_USE_QSTRINGBUILDER
        QT_USE_FAST_OPERATOR_PLUS
        QT_DISABLE_DEPRECATED_BEFORE=0x050500
        SIHHURI_VERSION="${PROJECT_VERSION}"
        SIHHURI_CONFIGFILE="${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT_NAME}.json"
        SIHHURI_TRANSDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
        $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
)

if(ENABLE_MAINTAINER_FLAGS)
    target_compile_definitions(sihhuri
        PRIVATE
            QT_DEPRECATED_WARNINGS
    )
    target_compile_options(sihhuri
        PRIVATE
            -Wall
            -Wcast-align
            -Wno-uninitialized
            -Wempty-body
            -Wformat-security
            -Wformat
            -Winit-self
    )
endif(ENABLE_MAINTAINER_FLAGS)

install(TARGETS sihhuri DESTINATION ${CMAKE_INSTALL_BINDIR})
