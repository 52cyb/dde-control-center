cmake_minimum_required(VERSION 3.18)
set(BIN_NAME dde-security-question-helper)

file(GLOB SRCS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/operation/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/operation/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/security-question-helper.qrc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../plugin-accounts/operation/pwqualitymanager.cpp"
)

find_package(${QT_NS} COMPONENTS Quick Gui DBus REQUIRED)
find_package(${DTK_NS} REQUIRED COMPONENTS Core Gui)
pkg_check_modules(DEEPIN_PW_CHECK REQUIRED libdeepin_pw_check)
pkg_check_modules(LIBCRYPT REQUIRED libcrypt)

add_executable(${BIN_NAME}
    ${SRCS}
)

target_include_directories(${BIN_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/operation
    ../plugin-accounts/operation
)

target_link_libraries(${BIN_NAME} PRIVATE
    ${QT_NS}::Core
    ${QT_NS}::Gui
    ${QT_NS}::Quick
    ${QT_NS}::DBus
    ${DTK_NS}::Core
    ${DTK_NS}::Gui
    ${DEEPIN_PW_CHECK_LIBRARIES}
    ${LIBCRYPT_LIBRARIES}
)

install(TARGETS ${BIN_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
