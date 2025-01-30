# SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
# SPDX-License-Identifier: GPL-3.0-or-later

set(sihhuri_TS_FILES
    sihhuri.ts
    sihhuri_de.ts
)

set_source_files_properties(${sihhuri_TS_FILES}
    PROPERTIES OUTPUT_LOCATION "${CMAKE_BINARY_DIR}/translations"
)

qt_add_lupdate(sihhuri
    TS_FILES ${sihhuri_TS_FILES}
    OPTIONS -no-obsolete -locations none -source-langauge en
)

qt_add_lrelease(sihhuri
    TS_FILES ${sihhuri_TS_FILES}
    QM_FILES_OUTPUT_VARIABLE sihhuri_QM_FILES
    OPTIONS -idbased
)

install(FILES ${sihhuri_QM_FILES} DESTINATION ${CMAKE_INSTALL_LOCALEDIR})
