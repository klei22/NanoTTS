cmake_minimum_required(VERSION 3.16)
project(NanoTTS VERSION 0.5.0 LANGUAGES C)

option(NANOTTS_BUILD_CLI "Build the command-line tool" ON)
option(NANOTTS_BUILD_TESTS "Build tests" ON)
option(NANOTTS_BUILD_EXAMPLES "Build integration examples" ON)
option(NANOTTS_BUILD_PWM "Build the optional PCM-to-PWM adapter" ON)
option(NANOTTS_BUILD_PCM "Build the optional raw PCM adapter" ON)
option(NANOTTS_ENABLE_STRICT_WARNINGS "Enable extra compiler warnings" ON)
option(NANOTTS_ENABLE_TEXT_FRONTEND "Build language text front ends" ON)
option(NANOTTS_ENABLE_LANG_ID "Build the Indonesian text module" ON)
option(NANOTTS_ENABLE_LANG_SW "Build the Kiswahili text module" ON)
option(NANOTTS_ENABLE_LANG_ES "Build the Spanish text module" ON)
option(NANOTTS_ENABLE_LANG_MS "Build the Malay text module" ON)
option(NANOTTS_ENABLE_LANG_MI "Build the Māori text module" ON)
option(NANOTTS_ENABLE_LANG_HAW "Build the Hawaiian text module" ON)
option(NANOTTS_USE_LIBM "Use system libm for trigonometry and pitch math" ON)

set(NANOTTS_MAX_EVENTS "512" CACHE STRING
    "Maximum number of parsed phone events buffered in one utterance")
set(NANOTTS_CONTEXT_BYTES "3072" CACHE STRING
    "Public opaque context size in bytes; must fit the selected configuration")
set(NANOTTS_AUDIO_BLOCK "128" CACHE STRING
    "PCM samples delivered per output callback block")

foreach(value_name IN ITEMS NANOTTS_MAX_EVENTS NANOTTS_CONTEXT_BYTES NANOTTS_AUDIO_BLOCK)
    if(NOT ${value_name} MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "${value_name} must be a positive integer")
    endif()
endforeach()

if(NANOTTS_ENABLE_TEXT_FRONTEND AND
   NOT NANOTTS_ENABLE_LANG_ID AND NOT NANOTTS_ENABLE_LANG_SW AND
   NOT NANOTTS_ENABLE_LANG_ES AND NOT NANOTTS_ENABLE_LANG_MS AND
   NOT NANOTTS_ENABLE_LANG_MI AND NOT NANOTTS_ENABLE_LANG_HAW)
    message(FATAL_ERROR
        "Text frontend enabled with no language modules; disable "
        "NANOTTS_ENABLE_TEXT_FRONTEND or enable at least one language")
endif()

set(NANOTTS_SOURCES
    src/nanotts.c
    src/nanotts_language.c
    src/nanotts_prosody.c
    src/nanotts_ipa.c
    src/nanotts_voice.c
    src/nanotts_synth.c)

if(NANOTTS_ENABLE_TEXT_FRONTEND)
    list(APPEND NANOTTS_SOURCES src/lang/nanotts_lang_common.c)
    if(NANOTTS_ENABLE_LANG_ID)
        list(APPEND NANOTTS_SOURCES src/lang/nanotts_lang_id.c)
    endif()
    if(NANOTTS_ENABLE_LANG_SW)
        list(APPEND NANOTTS_SOURCES src/lang/nanotts_lang_sw.c)
    endif()
    if(NANOTTS_ENABLE_LANG_ES)
        list(APPEND NANOTTS_SOURCES src/lang/nanotts_lang_es.c)
    endif()
    if(NANOTTS_ENABLE_LANG_MS)
        list(APPEND NANOTTS_SOURCES src/lang/nanotts_lang_ms.c)
    endif()
    if(NANOTTS_ENABLE_LANG_MI OR NANOTTS_ENABLE_LANG_HAW)
        list(APPEND NANOTTS_SOURCES src/lang/nanotts_lang_polynesian.c)
    endif()
    if(NANOTTS_ENABLE_LANG_MI)
        list(APPEND NANOTTS_SOURCES src/lang/nanotts_lang_mi.c)
    endif()
    if(NANOTTS_ENABLE_LANG_HAW)
        list(APPEND NANOTTS_SOURCES src/lang/nanotts_lang_haw.c)
    endif()
endif()

set(NANOTTS_LANGUAGE_SUMMARY " IPA only")
if(NANOTTS_ENABLE_TEXT_FRONTEND)
    set(NANOTTS_LANGUAGE_SUMMARY "")
    foreach(pair IN ITEMS
            "ID;id" "SW;sw" "ES;es" "MS;ms" "MI;mi" "HAW;haw")
        list(GET pair 0 token)
        list(GET pair 1 code)
        if(NANOTTS_ENABLE_LANG_${token})
            string(APPEND NANOTTS_LANGUAGE_SUMMARY " ${code}")
        endif()
    endforeach()
endif()
message(STATUS "NanoTTS text modules:${NANOTTS_LANGUAGE_SUMMARY}")

add_library(nanotts ${NANOTTS_SOURCES})
add_library(NanoTTS::NanoTTS ALIAS nanotts)
add_library(nanotts::nanotts ALIAS nanotts)

set_target_properties(nanotts PROPERTIES
    OUTPUT_NAME nanotts
    EXPORT_NAME NanoTTS
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

target_include_directories(nanotts
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(nanotts PUBLIC c_std_99)

# Context-affecting definitions are PUBLIC so a custom build gives consumers
# exactly the same nanotts_t layout as the library.
target_compile_definitions(nanotts
    PUBLIC
        NANOTTS_MAX_EVENTS=${NANOTTS_MAX_EVENTS}
        NANOTTS_CONTEXT_BYTES=${NANOTTS_CONTEXT_BYTES}
    PRIVATE
        NANOTTS_AUDIO_BLOCK=${NANOTTS_AUDIO_BLOCK}
        NANOTTS_ENABLE_TEXT_FRONTEND=$<BOOL:${NANOTTS_ENABLE_TEXT_FRONTEND}>
        NANOTTS_ENABLE_LANG_ID=$<BOOL:${NANOTTS_ENABLE_LANG_ID}>
        NANOTTS_ENABLE_LANG_SW=$<BOOL:${NANOTTS_ENABLE_LANG_SW}>
        NANOTTS_ENABLE_LANG_ES=$<BOOL:${NANOTTS_ENABLE_LANG_ES}>
        NANOTTS_ENABLE_LANG_MS=$<BOOL:${NANOTTS_ENABLE_LANG_MS}>
        NANOTTS_ENABLE_LANG_MI=$<BOOL:${NANOTTS_ENABLE_LANG_MI}>
        NANOTTS_ENABLE_LANG_HAW=$<BOOL:${NANOTTS_ENABLE_LANG_HAW}>
        NANOTTS_USE_LIBM=$<BOOL:${NANOTTS_USE_LIBM}>)

if(NANOTTS_ENABLE_STRICT_WARNINGS)
    if(MSVC)
        target_compile_options(nanotts PRIVATE /W4)
    else()
        target_compile_options(nanotts PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endif()

if(NANOTTS_USE_LIBM AND NOT MSVC)
    target_link_libraries(nanotts PUBLIC m)
endif()

if(NANOTTS_BUILD_PWM)
    add_library(nanotts_pwm src/output/nanotts_pwm.c)
    add_library(NanoTTS::PWM ALIAS nanotts_pwm)
    add_library(nanotts::pwm ALIAS nanotts_pwm)
    set_target_properties(nanotts_pwm PROPERTIES
        OUTPUT_NAME nanotts_pwm
        EXPORT_NAME PWM
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR})
    target_link_libraries(nanotts_pwm PUBLIC NanoTTS::NanoTTS)
    target_compile_features(nanotts_pwm PUBLIC c_std_99)
    if(NANOTTS_ENABLE_STRICT_WARNINGS)
        if(MSVC)
            target_compile_options(nanotts_pwm PRIVATE /W4)
        else()
            target_compile_options(nanotts_pwm PRIVATE
                -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        endif()
    endif()
endif()


if(NANOTTS_BUILD_PCM)
    add_library(nanotts_pcm src/output/nanotts_pcm.c)
    add_library(NanoTTS::PCM ALIAS nanotts_pcm)
    add_library(nanotts::pcm ALIAS nanotts_pcm)
    set_target_properties(nanotts_pcm PROPERTIES
        OUTPUT_NAME nanotts_pcm
        EXPORT_NAME PCM
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR})
    target_link_libraries(nanotts_pcm PUBLIC NanoTTS::NanoTTS)
    target_compile_features(nanotts_pcm PUBLIC c_std_99)
    if(NANOTTS_ENABLE_STRICT_WARNINGS)
        if(MSVC)
            target_compile_options(nanotts_pcm PRIVATE /W4)
        else()
            target_compile_options(nanotts_pcm PRIVATE
                -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        endif()
    endif()
endif()

if(NANOTTS_BUILD_CLI)
    add_executable(nanotts_cli tools/nanotts_cli.c)
    set_target_properties(nanotts_cli PROPERTIES OUTPUT_NAME nanotts)
    target_link_libraries(nanotts_cli PRIVATE NanoTTS::NanoTTS)
    if(NANOTTS_BUILD_PWM)
        target_link_libraries(nanotts_cli PRIVATE NanoTTS::PWM)
        target_compile_definitions(nanotts_cli PRIVATE NANOTTS_CLI_HAVE_PWM=1)
    else()
        target_compile_definitions(nanotts_cli PRIVATE NANOTTS_CLI_HAVE_PWM=0)
    endif()
    if(NANOTTS_BUILD_PCM)
        target_link_libraries(nanotts_cli PRIVATE NanoTTS::PCM)
        target_compile_definitions(nanotts_cli PRIVATE NANOTTS_CLI_HAVE_PCM=1)
    else()
        target_compile_definitions(nanotts_cli PRIVATE NANOTTS_CLI_HAVE_PCM=0)
    endif()
endif()

if(NANOTTS_BUILD_EXAMPLES)
    add_executable(nanotts_mcu_example examples/nanotts_mcu_callback.c)
    target_link_libraries(nanotts_mcu_example PRIVATE NanoTTS::NanoTTS)
    if(NANOTTS_BUILD_PWM)
        add_executable(nanotts_pwm_example examples/nanotts_mcu_pwm.c)
        target_link_libraries(nanotts_pwm_example PRIVATE NanoTTS::PWM)
    endif()
    if(NANOTTS_ENABLE_TEXT_FRONTEND)
        add_executable(nanotts_language_example examples/nanotts_language_select.c)
        target_link_libraries(nanotts_language_example PRIVATE NanoTTS::NanoTTS)
    endif()
endif()

if(NANOTTS_BUILD_TESTS)
    enable_testing()
    set(NANOTTS_TESTS api ipa synth)
    if(NANOTTS_ENABLE_TEXT_FRONTEND)
        list(APPEND NANOTTS_TESTS languages)
    endif()
    if(NANOTTS_BUILD_PWM)
        list(APPEND NANOTTS_TESTS pwm)
    endif()
    if(NANOTTS_BUILD_PCM)
        list(APPEND NANOTTS_TESTS pcm)
    endif()
    foreach(test_name IN LISTS NANOTTS_TESTS)
        add_executable(test_${test_name} tests/test_${test_name}.c)
        if(test_name STREQUAL "pwm")
            target_link_libraries(test_${test_name} PRIVATE NanoTTS::PWM)
        elseif(test_name STREQUAL "pcm")
            target_link_libraries(test_${test_name} PRIVATE NanoTTS::PCM)
        else()
            target_link_libraries(test_${test_name} PRIVATE NanoTTS::NanoTTS)
        endif()
        if(MSVC)
            target_compile_options(test_${test_name} PRIVATE /UNDEBUG)
        else()
            target_compile_options(test_${test_name} PRIVATE -UNDEBUG)
        endif()
        add_test(NAME ${test_name} COMMAND test_${test_name})
    endforeach()
    target_compile_definitions(test_api PRIVATE
        NANOTTS_EXPECT_TEXT_FRONTEND=$<BOOL:${NANOTTS_ENABLE_TEXT_FRONTEND}>
        NANOTTS_EXPECT_LANG_ID=$<AND:$<BOOL:${NANOTTS_ENABLE_TEXT_FRONTEND}>,$<BOOL:${NANOTTS_ENABLE_LANG_ID}>>
        NANOTTS_EXPECT_LANG_SW=$<AND:$<BOOL:${NANOTTS_ENABLE_TEXT_FRONTEND}>,$<BOOL:${NANOTTS_ENABLE_LANG_SW}>>
        NANOTTS_EXPECT_LANG_ES=$<AND:$<BOOL:${NANOTTS_ENABLE_TEXT_FRONTEND}>,$<BOOL:${NANOTTS_ENABLE_LANG_ES}>>
        NANOTTS_EXPECT_LANG_MS=$<AND:$<BOOL:${NANOTTS_ENABLE_TEXT_FRONTEND}>,$<BOOL:${NANOTTS_ENABLE_LANG_MS}>>
        NANOTTS_EXPECT_LANG_MI=$<AND:$<BOOL:${NANOTTS_ENABLE_TEXT_FRONTEND}>,$<BOOL:${NANOTTS_ENABLE_LANG_MI}>>
        NANOTTS_EXPECT_LANG_HAW=$<AND:$<BOOL:${NANOTTS_ENABLE_TEXT_FRONTEND}>,$<BOOL:${NANOTTS_ENABLE_LANG_HAW}>>)
    if(TARGET test_languages)
        target_compile_definitions(test_languages PRIVATE
            NANOTTS_EXPECT_LANG_ID=$<BOOL:${NANOTTS_ENABLE_LANG_ID}>
            NANOTTS_EXPECT_LANG_SW=$<BOOL:${NANOTTS_ENABLE_LANG_SW}>
            NANOTTS_EXPECT_LANG_ES=$<BOOL:${NANOTTS_ENABLE_LANG_ES}>
            NANOTTS_EXPECT_LANG_MS=$<BOOL:${NANOTTS_ENABLE_LANG_MS}>
            NANOTTS_EXPECT_LANG_MI=$<BOOL:${NANOTTS_ENABLE_LANG_MI}>
            NANOTTS_EXPECT_LANG_HAW=$<BOOL:${NANOTTS_ENABLE_LANG_HAW}>)
    endif()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(NANOTTS_INSTALL_TARGETS nanotts)
if(NANOTTS_BUILD_PWM)
    list(APPEND NANOTTS_INSTALL_TARGETS nanotts_pwm)
endif()
if(NANOTTS_BUILD_PCM)
    list(APPEND NANOTTS_INSTALL_TARGETS nanotts_pcm)
endif()
install(TARGETS ${NANOTTS_INSTALL_TARGETS}
    EXPORT NanoTTSTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(NANOTTS_BUILD_CLI)
    install(TARGETS nanotts_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE README.md PROVENANCE.md CHANGELOG.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/NanoTTS)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/NanoTTS/docs)

configure_package_config_file(
    cmake/nanottsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NanoTTSConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NanoTTS)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/NanoTTSConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(EXPORT NanoTTSTargets
    NAMESPACE NanoTTS::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NanoTTS)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/NanoTTSConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/NanoTTSConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NanoTTS)

set(CPACK_PACKAGE_NAME "NanoTTS")
set(CPACK_PACKAGE_VENDOR "NanoTTS contributors")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_SOURCE_PACKAGE_FILE_NAME "NanoTTS-${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Compact multilingual IPA/text-to-speech library for MCUs")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_SOURCE_GENERATOR "ZIP;TGZ")
set(CPACK_SOURCE_IGNORE_FILES
    "/build[^/]*/"
    "/[.]git/"
    "/validation/"
    "/dist/"
    "/out/"
    "~$"
    "[.]wav$"
    "[.]pcm$"
    "[.]pcm16le$"
    "[.]raw$"
    "[.]pwm$"
    "[.]pwm16le$"
    "[.]png$")
include(CPack)
