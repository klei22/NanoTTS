cmake_minimum_required(VERSION 3.16)
project(idtts VERSION 0.2.1 LANGUAGES C)

option(IDTTS_BUILD_CLI "Build the WAV command-line tool" ON)
option(IDTTS_BUILD_TESTS "Build tests" ON)
option(IDTTS_BUILD_EXAMPLES "Build integration examples" ON)
option(IDTTS_ENABLE_STRICT_WARNINGS "Enable extra compiler warnings" ON)
option(IDTTS_ENABLE_TEXT_FRONTEND "Build the independent Indonesian text front end" ON)
option(IDTTS_USE_LIBM "Use system libm for trigonometry and pitch math" ON)

set(IDTTS_MAX_EVENTS "512" CACHE STRING
    "Maximum number of parsed phone events buffered in one utterance")
set(IDTTS_CONTEXT_BYTES "3072" CACHE STRING
    "Public opaque context size in bytes; must fit the selected configuration")
set(IDTTS_AUDIO_BLOCK "128" CACHE STRING
    "PCM samples delivered per output callback block")

foreach(value_name IN ITEMS IDTTS_MAX_EVENTS IDTTS_CONTEXT_BYTES IDTTS_AUDIO_BLOCK)
    if(NOT ${value_name} MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "${value_name} must be a positive integer")
    endif()
endforeach()

set(IDTTS_SOURCES
    src/idtts.c
    src/idtts_ipa.c
    src/idtts_voice.c
    src/idtts_synth.c)
if(IDTTS_ENABLE_TEXT_FRONTEND)
    list(APPEND IDTTS_SOURCES src/idtts_g2p.c)
endif()

add_library(idtts ${IDTTS_SOURCES})
add_library(idtts::idtts ALIAS idtts)

set_target_properties(idtts PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

target_include_directories(idtts
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(idtts PUBLIC c_std_99)

# Context-affecting definitions are PUBLIC so consumers of a custom build see
# exactly the same idtts_t layout as the library.
target_compile_definitions(idtts
    PUBLIC
        IDTTS_MAX_EVENTS=${IDTTS_MAX_EVENTS}
        IDTTS_CONTEXT_BYTES=${IDTTS_CONTEXT_BYTES}
    PRIVATE
        IDTTS_AUDIO_BLOCK=${IDTTS_AUDIO_BLOCK}
        IDTTS_ENABLE_TEXT_FRONTEND=$<BOOL:${IDTTS_ENABLE_TEXT_FRONTEND}>
        IDTTS_USE_LIBM=$<BOOL:${IDTTS_USE_LIBM}>)

if(IDTTS_ENABLE_STRICT_WARNINGS)
    if(MSVC)
        target_compile_options(idtts PRIVATE /W4)
    else()
        target_compile_options(idtts PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endif()

if(IDTTS_USE_LIBM AND NOT MSVC)
    target_link_libraries(idtts PUBLIC m)
endif()

if(IDTTS_BUILD_CLI)
    add_executable(idtts_cli tools/idtts_cli.c)
    set_target_properties(idtts_cli PROPERTIES OUTPUT_NAME idtts)
    target_link_libraries(idtts_cli PRIVATE idtts::idtts)
endif()

if(IDTTS_BUILD_EXAMPLES)
    add_executable(idtts_mcu_example examples/mcu_callback.c)
    target_link_libraries(idtts_mcu_example PRIVATE idtts::idtts)
endif()

if(IDTTS_BUILD_TESTS)
    enable_testing()
    set(IDTTS_TESTS api ipa synth)
    if(IDTTS_ENABLE_TEXT_FRONTEND)
        list(APPEND IDTTS_TESTS g2p)
    endif()
    foreach(test_name IN LISTS IDTTS_TESTS)
        add_executable(test_${test_name} tests/test_${test_name}.c)
        target_link_libraries(test_${test_name} PRIVATE idtts::idtts)
        # CMake defines NDEBUG for release-like configurations. These tests use
        # assert(), so explicitly keep assertions active in every build mode.
        if(MSVC)
            target_compile_options(test_${test_name} PRIVATE /UNDEBUG)
        else()
            target_compile_options(test_${test_name} PRIVATE -UNDEBUG)
        endif()
        if(test_name STREQUAL "api")
            target_compile_definitions(test_api PRIVATE
                IDTTS_EXPECT_TEXT_FRONTEND=$<BOOL:${IDTTS_ENABLE_TEXT_FRONTEND}>)
        endif()
        add_test(NAME ${test_name} COMMAND test_${test_name})
    endforeach()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS idtts
    EXPORT idttsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(IDTTS_BUILD_CLI)
    install(TARGETS idtts_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE README.md PROVENANCE.md CHANGELOG.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/idtts)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/idtts/docs)

configure_package_config_file(
    cmake/idttsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/idttsConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/idtts)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/idttsConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(EXPORT idttsTargets
    NAMESPACE idtts::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/idtts)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/idttsConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/idttsConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/idtts)

set(CPACK_PACKAGE_NAME "idtts")
set(CPACK_PACKAGE_VENDOR "idtts contributors")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_SOURCE_PACKAGE_FILE_NAME "idtts-${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Compact Indonesian IPA/text-to-speech library")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_SOURCE_GENERATOR "ZIP;TGZ")
set(CPACK_SOURCE_IGNORE_FILES
    "/build[^/]*/"
    "/\.git/"
    "~$"
    "\.wav$"
    "\.png$")
include(CPack)
