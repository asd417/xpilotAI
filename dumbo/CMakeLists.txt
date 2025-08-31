cmake_minimum_required(VERSION 3.16)
project(XPilotBOT)

# Put executables under <source>/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin")
# For multi-config generators (VS/Xcode), mirror the same dir:
foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} "${CMAKE_CURRENT_SOURCE_DIR}/../build")
endforeach()

#set(CAI_INCLUDE_DIR "$ENV{HOME}/xpilot-ai/library/c"  CACHE PATH "Path to cAI headers")
#set(CAI_LIB_DIR     "$ENV{HOME}/xpilot-ai/binaries" CACHE PATH "Path to folder containing libcAI.so")
set(CAI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../include"  CACHE PATH "Path to cAI headers")
set(CAI_LIB_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/../lib" CACHE PATH "Path to folder containing libcAI.so")

find_library(CAI_LIBRARY
  NAMES cAI libcAI
  PATHS "${CAI_LIB_DIR}"
  NO_DEFAULT_PATH
)

if(NOT CAI_LIBRARY)
  message(FATAL_ERROR "Could not find libcAI.so in ${CAI_LIB_DIR}. Set -DCAI_LIB_DIR=/path/to/binaries")
endif()

# --- Target ---
add_executable(Dumbo Dumbo.c)

# Headers
target_include_directories(Dumbo PRIVATE "${CAI_INCLUDE_DIR}")

# Link the shared lib (absolute path)
target_link_libraries(Dumbo PRIVATE "${CAI_LIBRARY}")

# Embed rpath so runtime can find libcAI.so without LD_LIBRARY_PATH
set_target_properties(Dumbo PROPERTIES
  BUILD_RPATH "${CAI_LIB_DIR}"
  INSTALL_RPATH "${CAI_LIB_DIR}"
)

# Nice-to-have warnings / standard
set(CMAKE_C_STANDARD 99)
target_compile_options(Dumbo PRIVATE -Wall -Wextra -Wpedantic)