add_custom_target(service SOURCES
    sihhuri.service.in
    sihhuri.timer
)

configure_file(sihhuri.service.in ${CMAKE_BINARY_DIR}/service/sihhuri.service @ONLY)

install(FILES
    ${CMAKE_BINARY_DIR}/service/sihhuri.service
    sihhuri.timer
    DESTINATION ${SYSTEMD_UNIT_DIR}
)
