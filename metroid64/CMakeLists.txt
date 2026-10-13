file(GLOB_RECURSE METROID64_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

add_executable(metroid64 ${METROID64_SOURCES})
target_include_directories(metroid64 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(metroid64 PRIVATE F3DEX_GBI_2)
target_link_libraries(metroid64 PRIVATE libultraship)
set_target_properties(metroid64 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/metroid64)

if(MSVC)
    # NOMINMAX: windows.h (via libultraship) would otherwise turn std::min/std::max into macros.
    target_compile_definitions(metroid64 PRIVATE _USE_MATH_DEFINES NOMINMAX)
    # Windowed app without a console, still entering through the standard main().
    set_target_properties(metroid64 PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_options(metroid64 PRIVATE /ENTRY:mainCRTStartup)
    set_target_properties(metroid64 PROPERTIES
        MSVC_RUNTIME_LIBRARY "$<IF:$<CONFIG:Debug>,MultiThreadedDebug,MultiThreaded>")
endif()

# metroid64.o2r only carries libultraship's renderer shaders; all game data comes from the
# player's own Metroid ROM at runtime.
set(METROID64_O2R ${CMAKE_BINARY_DIR}/metroid64/metroid64.o2r)
file(GLOB_RECURSE LUS_SHADERS CONFIGURE_DEPENDS ${LUS_DIR}/src/fast/shaders/*)
add_custom_command(
    OUTPUT ${METROID64_O2R}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/metroid64
    COMMAND ${CMAKE_COMMAND} -E rm -f ${METROID64_O2R}
    COMMAND ${CMAKE_COMMAND} -E chdir ${LUS_DIR}/src/fast ${CMAKE_COMMAND} -E tar cf ${METROID64_O2R} --format=zip shaders
    DEPENDS ${LUS_SHADERS}
    COMMENT "Packing metroid64.o2r"
)
add_custom_target(Metroid64Archive ALL DEPENDS ${METROID64_O2R})
add_dependencies(metroid64 Metroid64Archive)

install(TARGETS metroid64 DESTINATION . COMPONENT metroid64)
install(FILES ${METROID64_O2R} DESTINATION . COMPONENT metroid64)
