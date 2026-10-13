# Lockstep test: the original game on an emulated NES versus the C++ port. Not part of the game.
file(GLOB LOCKSTEP_GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../src/game/*.cpp)
add_executable(lockstep
    main.cpp
    OriginalCpu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/nes/Cpu6502.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/nes/Nes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/nes/Apu.cpp
    ${LOCKSTEP_GAME_SOURCES}
)
target_include_directories(lockstep PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_compile_features(lockstep PRIVATE cxx_std_20)
