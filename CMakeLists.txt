cmake_minimum_required(VERSION 3.30)
project(wcs)

set(CMAKE_CXX_STANDARD 23)

# -------------------------------
# List of common WinAPI libraries
set(COMMON_LIBS Shlwapi ws2_32 advapi32 Crypt32)

# -------
# Helpers
file(GLOB_RECURSE HELPER_FILES ${PROJECT_SOURCE_DIR}/helpers/*.h ${PROJECT_SOURCE_DIR}/helpers/*.cpp)
add_library(helpers ${HELPER_FILES})
target_include_directories(helpers PUBLIC ${PROJECT_SOURCE_DIR}/helpers)
set_target_properties(helpers PROPERTIES LINKER_LANGUAGE CXX)

# -----------------------------------
# Actual client and server submodules
add_subdirectory("${PROJECT_SOURCE_DIR}/client" "${PROJECT_SOURCE_DIR}/client/bin")
target_link_libraries(client ${COMMON_LIBS} helpers)

add_subdirectory("${PROJECT_SOURCE_DIR}/server" "${PROJECT_SOURCE_DIR}/server/bin")
target_link_libraries(server ${COMMON_LIBS} helpers)
