# SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
# SPDX-License-Identifier: GPL-3.0-or-later

find_program(LRELEASE_CMD_PATH NAMES lrelease-qt5 lrelease REQUIRED)
set(LRELEASE_CMD ${LRELEASE_CMD_PATH})

message(STATUS "Found lrelease at ${LRELEASE_CMD}")

set(LANGS de)

set(QM_FILES ${CMAKE_CURRENT_BINARY_DIR}/sihhuri.qm)
set(TS_FILES sihhuri.ts)

set(LRELEASE_OPTIONS "-idbased" "-silent")

foreach(l ${LANGS})
    set(TRANS_ARGS ${TRANS_ARGS} COMMAND ${LRELEASE_CMD} ${LRELEASE_OPTIONS} sihhuri_${l}.ts -qm ${CMAKE_CURRENT_BINARY_DIR}/sihhuri_${l}.qm)
    list(APPEND QM_FILES ${CMAKE_CURRENT_BINARY_DIR}/sihhuri_${l}.qm)
    list(APPEND TS_FILES sihhuri_${l}.ts)
endforeach(l ${LANGS})

add_custom_target(translations ALL ${LRELEASE_CMD} ${LRELEASE_OPTIONS} sihhuri.ts -qm ${CMAKE_CURRENT_BINARY_DIR}/sihhuri.qm
    ${TRANS_ARGS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling translation files"
    SOURCES ${TS_FILES}
)

install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_LOCALEDIR})
