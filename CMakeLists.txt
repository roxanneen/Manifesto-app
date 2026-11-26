cmake_minimum_required(VERSION 3.14)
project(ManifestoUiPlugin LANGUAGES CXX)

if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found. Set LOGOS_MODULE_BUILDER_ROOT.")
endif()

# metadata.json is the single source of truth for the module name and version.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json" METADATA_JSON)
string(JSON MODULE_NAME GET ${METADATA_JSON} name)
string(JSON MODULE_VERSION GET ${METADATA_JSON} version)

# The manifesto text is baked in at build time from src/manifesto.txt. Its
# sha256 is what every signature names, so the file is the signed version.
set(MANIFESTO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/manifesto.txt")
set(MANIFESTO_GEN "${CMAKE_CURRENT_BINARY_DIR}/manifesto_text.inc")
file(READ "${MANIFESTO_SRC}" MANIFESTO_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," MANIFESTO_BYTES "${MANIFESTO_HEX}")
file(WRITE "${MANIFESTO_GEN}" "${MANIFESTO_BYTES}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MANIFESTO_SRC}")

logos_module(
    NAME ${MODULE_NAME}
    REP_FILE src/manifesto_ui.rep
    SOURCES
        src/manifesto_ui_backend.h
        src/manifesto_ui_backend.cpp
        src/sigcheck.h
        src/sigcheck.cpp
    INCLUDE_DIRS
        src
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(${MODULE_NAME}_module_plugin PRIVATE
    MANIFESTO_BUILD_ID="${MODULE_NAME}/${MODULE_VERSION}"
)

# libsecp256k1: public key recovery, the whole of signature checking. Comes
# from metadata.json nix.packages.runtime; the portable bundler ships the
# dylib next to the plugin.
find_path(SECP256K1_INCLUDE_DIR secp256k1_recovery.h REQUIRED)
find_library(SECP256K1_LIBRARY NAMES secp256k1 libsecp256k1 REQUIRED)
target_include_directories(${MODULE_NAME}_module_plugin PRIVATE ${SECP256K1_INCLUDE_DIR})
target_link_libraries(${MODULE_NAME}_module_plugin PRIVATE ${SECP256K1_LIBRARY})
