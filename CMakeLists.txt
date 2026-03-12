# CMakeList.txt : CMake project for Liminescence, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.20)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Liminescence")

# Fetch raylib from GitHub
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# Add source to this project's executable.
add_executable (Liminescence "src/main.cpp" "src/Liminescence.h" "src/Chunk.h" "src/Chunk.cpp" "src/World.h" "src/World.cpp" "src/BlockTypes.h")

# Link raylib to your project
target_link_libraries(Liminescence PRIVATE raylib)

add_custom_command(TARGET Liminescence POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:Liminescence>/assets
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Liminescence PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
