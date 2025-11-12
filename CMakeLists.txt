cmake_minimum_required(VERSION 3.29.2)

set(PROGRAM_VERSION "KalaGraphics 1.0.0")
set(PROGRAM_VERSION_NUMBER 1.0.0.0)

project("KalaGraphics" VERSION ${PROGRAM_VERSION_NUMBER} LANGUAGES C CXX)

# Adds a '_' prefix in front of the created binary files if set to TRUE
set(KALA_PRERELEASE TRUE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Runtime lib handling for MSVC
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Platform Detection
if (WIN32)
    message(STATUS "[KALAGRAPHICS] Platform = Windows")
else()
    message(FATAL_ERROR "[KALAGRAPHICS] Unsupported platform. Only Windows is supported.")
endif()

# Build Type Detection
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(IS_DEBUG TRUE)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(IS_RELEASE TRUE)
else()
    message(FATAL_ERROR "Unknown CMAKE_BUILD_TYPE: '${CMAKE_BUILD_TYPE}'! Must be Debug, Release, or RelWithDebInfo.")
endif()

# Dependencies
find_package(OpenGL REQUIRED)
# find_package(Vulkan REQUIRED)

# Directory Setup
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(EXT_SHARED_DIR "${CMAKE_SOURCE_DIR}/_external_shared")

# Source Files
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*/*.cpp"
)

# SHARED LIBRARY (.dll)
add_library(KalaGraphics SHARED ${SOURCE_FILES})

# Enable exception handling for MSVC
if (MSVC)
    target_compile_options(KalaGraphics PRIVATE /EHsc)
endif()
	
# Get suffix
if (IS_RELEASE)
	set(KALA_SUFFIX "")
else()
	set(KALA_SUFFIX "d")
endif()

# Set output name with renderer suffix and debug postfix
set_target_properties(KalaGraphics PROPERTIES
	OUTPUT_NAME "KalaGraphics${PROJECT_VERSION_MAJOR}${KALA_SUFFIX}"
)

# Includes
file(GLOB_RECURSE HEADERS
	configure_depends
	"${CMAKE_SOURCE_DIR}/include/*.hpp"
)
target_sources(KalaGraphics PRIVATE ${HEADERS})
target_include_directories(KalaGraphics PRIVATE
	"${INCLUDE_DIR}"
	"${EXT_SHARED_DIR}"
)

# Preprocessor Defines
target_compile_definitions(KalaGraphics PRIVATE 
	LIB_EXPORT
	WIN32_LEAN_AND_MEAN
	NOMINMAX)
	
# Link libraries
target_link_libraries(KalaGraphics PRIVATE
	OpenGL::GL
	${EXT_SHARED_DIR}/Vulkan/vulkan-1.lib
)
	
# Expose version string
add_compile_definitions(
	PROGRAM_VERSION="${PROGRAM_VERSION}"
)

# Hide console in release mode
#if(IS_RELEASE)
#    set_target_properties(KalaGraphics PROPERTIES WIN32_EXECUTABLE TRUE)
#    if (MSVC)
#        set_target_properties(KalaGraphics PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
#    endif()
#endif()

# Copy important files
set(COPY_TARGETS README.md LICENSE.md)
foreach(FILE ${COPY_TARGETS})
add_custom_command(TARGET KalaGraphics POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -f 
        "$<TARGET_FILE_DIR:KalaGraphics>/${FILE}"
    COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_SOURCE_DIR}/${FILE}"
		"$<TARGET_FILE_DIR:KalaGraphics>/${FILE}"
)
endforeach()

# Copy entire docs folder
add_custom_command(TARGET KalaGraphics POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf
        "$<TARGET_FILE_DIR:KalaGraphics>/docs"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/docs"
        "$<TARGET_FILE_DIR:KalaGraphics>/docs"
)