cmake_minimum_required(VERSION 3.29)

project(JalapenoVK VERSION 1.0.0 LANGUAGES CXX C)

option(ENABLE_CPP20_MODULE "Enable C++20 module support for Vulkan" OFF)

if(ENABLE_CPP20_MODULE)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

find_package(glfw3         REQUIRED)
find_package(glm           REQUIRED)
find_package(Vulkan 1.4.335 REQUIRED)
find_package(tinyobjloader REQUIRED)
find_package(tinygltf      REQUIRED)
find_package(KTX           REQUIRED)
find_package(stb           REQUIRED)

set(STB_INCLUDEDIR ${stb_INCLUDE_DIRS})

# ---------------------------------------------------------------------------
# Vulkan-Hpp target (C++20 module or plain header fallback)
# ---------------------------------------------------------------------------
if(ENABLE_CPP20_MODULE)
    add_library(VulkanCppModule)
    add_library(Vulkan::cppm ALIAS VulkanCppModule)

    target_compile_definitions(VulkanCppModule PUBLIC
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
    )
    target_include_directories(VulkanCppModule PUBLIC "${Vulkan_INCLUDE_DIR}")
    target_link_libraries(VulkanCppModule PUBLIC Vulkan::Vulkan)
    set_target_properties(VulkanCppModule PROPERTIES CXX_STANDARD 20)
    target_sources(VulkanCppModule PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS "${Vulkan_INCLUDE_DIR}"
        FILES     "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
    )
    if(MSVC)
        target_compile_options(VulkanCppModule PRIVATE
            /std:c++latest /permissive- /Zc:__cplusplus /EHsc /Zc:preprocessor
        )
    endif()
else()
    add_library(VulkanCppModule INTERFACE)
    add_library(Vulkan::cppm ALIAS VulkanCppModule)
    target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
    target_compile_definitions(VulkanCppModule INTERFACE
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
    )
    target_include_directories(VulkanCppModule INTERFACE "${Vulkan_INCLUDE_DIR}")
endif()

# ---------------------------------------------------------------------------
# Shader compilation (Slang -> SPIR-V)
# Note: compiles each .slang as a separate SPV
# ---------------------------------------------------------------------------
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/JalapenoVK")

find_program(SLANGC_EXECUTABLE slangc HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)

file(GLOB SLANG_SHADER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.slang")

if(SLANG_SHADER_SOURCES)
    set(SHADERS_DIR "${OUTPUT_DIR}/shaders")
    foreach(SHADER ${SLANG_SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        add_custom_command(
            OUTPUT  "${SHADERS_DIR}/${SHADER_NAME}.spv"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADERS_DIR}"
            COMMAND ${SLANGC_EXECUTABLE} "${SHADER}"
                    -target spirv -profile spirv_1_4
                    -emit-spirv-directly
                    -fvk-use-entrypoint-name
                    -o "${SHADERS_DIR}/${SHADER_NAME}.spv"
            DEPENDS "${SHADER}"
            COMMENT "Compiling ${SHADER_NAME}"
            VERBATIM
        )
        list(APPEND SHADER_SPVS "${SHADERS_DIR}/${SHADER_NAME}.spv")
    endforeach()
    add_custom_target(shaders DEPENDS ${SHADER_SPVS})
else()
    message(STATUS "No shaders found — skipping shader compilation.")
    add_custom_target(shaders)
endif()

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
file(GLOB_RECURSE JALAPENO_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_executable(JalapenoVK ${JALAPENO_SOURCES})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${JALAPENO_SOURCES})

add_dependencies(JalapenoVK shaders)

set_target_properties(JalapenoVK PROPERTIES
    CXX_STANDARD 20
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)

target_compile_definitions(JalapenoVK PRIVATE
    GLM_ENABLE_EXPERIMENTAL
    _USE_MATH_DEFINES
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS  # treat VK_ERROR_OUT_OF_DATE_KHR as success
)

target_link_libraries(JalapenoVK PRIVATE
    Vulkan::cppm
    glm::glm
    glfw
    KTX::ktx
    tinygltf::tinygltf
    tinyobjloader::tinyobjloader
)
target_include_directories(JalapenoVK PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${STB_INCLUDEDIR}
)

if(ENABLE_CPP20_MODULE)
    target_compile_definitions(JalapenoVK PRIVATE USE_CPP20_MODULES=1)
endif()

# ---------------------------------------------------------------------------
# MSVC-specific settings
# ---------------------------------------------------------------------------
if(MSVC)
    target_compile_definitions(JalapenoVK PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
    target_compile_options(JalapenoVK PRIVATE
        /permissive- /Zc:__cplusplus /EHsc /W3 /MP /bigobj
    )
    if(${CMAKE_GENERATOR} MATCHES "Visual Studio.*")
        set_target_properties(JalapenoVK PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "${OUTPUT_DIR}"
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# Assets (copied at configure time)
# ---------------------------------------------------------------------------
set(ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
if(EXISTS "${ASSET_DIR}")
    file(COPY "${ASSET_DIR}/" DESTINATION "${OUTPUT_DIR}/assets")
    message(STATUS "Assets copied to ${OUTPUT_DIR}/assets")
else()
    message(STATUS "No assets folder found — skipping.")
endif()
