cmake_minimum_required(VERSION 3.25)

function(mark_dependency_system target)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES SYSTEM ON)
    endif()
endfunction()

find_or_add(yaml
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
)
mark_dependency_system(yaml-cpp)

find_or_add(cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    CMAKE_CACHE_VARS
        "CXXOPTS_STANDALONE_PROJECT=OFF"
)
mark_dependency_system(cxxopts)

find_or_add(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.4
)
mark_dependency_system(Catch2)
mark_dependency_system(Catch2WithMain)

if(NOT EMSCRIPTEN)
    set(_WOKI_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF)
    find_or_add(fastgltf
        NO_SYSTEM
        GIT_REPOSITORY https://github.com/spnda/fastgltf.git
        GIT_TAG v0.9.0
        DISABLE_INSTALL
        CMAKE_CACHE_VARS
            "FASTGLTF_ENABLE_TESTS=OFF"
            "FASTGLTF_ENABLE_EXAMPLES=OFF"
            "FASTGLTF_ENABLE_DOCS=OFF"
    )
    mark_dependency_system(fastgltf)

    find_or_add(meshoptimizer
        NO_SYSTEM
        GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
        GIT_TAG v0.25
        DISABLE_INSTALL
        CMAKE_CACHE_VARS
            "MESHOPT_BUILD_DEMO=OFF"
            "MESHOPT_BUILD_GLTFPACK=OFF"
    )
    mark_dependency_system(meshoptimizer)

    set(UNZIP_FOUND OFF CACHE BOOL "Use Assimp's pinned minizip sources" FORCE)
    find_or_add(assimp
        NO_SYSTEM
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v6.0.2
        DISABLE_INSTALL
        CMAKE_CACHE_VARS
            "ASSIMP_BUILD_TESTS=OFF"
            "ASSIMP_BUILD_ASSIMP_TOOLS=OFF"
            "ASSIMP_BUILD_SAMPLES=OFF"
            "ASSIMP_BUILD_ZLIB=ON"
            "ASSIMP_INSTALL=OFF"
            "ASSIMP_WARNINGS_AS_ERRORS=OFF"
            "ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=OFF"
            "ASSIMP_BUILD_MINIZIP=ON"
    )
    target_include_directories(assimp PRIVATE "${CDEPS_ROOT}/assimp-src/contrib/unzip")
    mark_dependency_system(assimp)

    include(FetchContent)
    FetchContent_Declare(woki_mikktspace
        GIT_REPOSITORY https://github.com/mmikk/MikkTSpace.git
        GIT_TAG 3e895b49d05ea07e4c2133156cfa94369e19e409
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(woki_mikktspace)
    add_library(woki_mikktspace STATIC "${woki_mikktspace_SOURCE_DIR}/mikktspace.c")
    target_include_directories(woki_mikktspace SYSTEM PUBLIC "${woki_mikktspace_SOURCE_DIR}")
    mark_dependency_system(woki_mikktspace)

    find_or_add(KTX
        NO_SYSTEM
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG 936b655d10fe75f900967f524ba31005bebcbb47
        RECURSE_SUBMODULES
        DISABLE_INSTALL
        CMAKE_CACHE_VARS
            "KTX_FEATURE_DOC=OFF"
            "KTX_FEATURE_GL_UPLOAD=OFF"
            "KTX_FEATURE_JNI=OFF"
            "KTX_FEATURE_KTX1=ON"
            "KTX_FEATURE_LOADTEST_APPS=OFF"
            "KTX_FEATURE_TESTS=OFF"
            "KTX_FEATURE_TOOLS=OFF"
            "KTX_FEATURE_VK_UPLOAD=OFF"
    )
    set(BUILD_SHARED_LIBS "${_WOKI_BUILD_SHARED_LIBS}")
    unset(_WOKI_BUILD_SHARED_LIBS)
    # KTX 4.4.2 applies this C++-only Clang option to bundled zstd.c when C and
    # C++ use different compiler families. Its ASTC target keeps its own flags.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        foreach(_WOKI_KTX_TARGET ktx ktx_read)
            get_target_property(_WOKI_KTX_OPTIONS ${_WOKI_KTX_TARGET} COMPILE_OPTIONS)
            list(FILTER _WOKI_KTX_OPTIONS EXCLUDE REGEX "ffp-model=precise")
            set_target_properties(${_WOKI_KTX_TARGET} PROPERTIES COMPILE_OPTIONS "${_WOKI_KTX_OPTIONS}")
        endforeach()
        unset(_WOKI_KTX_OPTIONS)
        unset(_WOKI_KTX_TARGET)
    endif()
    mark_dependency_system(ktx_read)

    include(FetchContent)
    FetchContent_Declare(woki_stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(woki_stb)
    add_library(woki_stb INTERFACE)
    target_include_directories(woki_stb SYSTEM INTERFACE "${woki_stb_SOURCE_DIR}")
endif()

if(NOT EMSCRIPTEN)
    find_package(LibArchive QUIET)
    if(NOT LibArchive_FOUND)
        find_or_add(LibArchive
            NO_SYSTEM
            GIT_REPOSITORY https://github.com/libarchive/libarchive.git
            GIT_TAG v3.7.7
            DISABLE_INSTALL
            CMAKE_CACHE_VARS
                "ENABLE_TEST=OFF"
                "ENABLE_WERROR=OFF"
                "ENABLE_INSTALL=OFF"
                "ENABLE_TAR=OFF"
                "ENABLE_CPIO=OFF"
                "ENABLE_CAT=OFF"
        )
    else()
        message(STATUS "Found LibArchive (system)")
    endif()

    if(TARGET archive)
        mark_dependency_system(archive)
    elseif(TARGET archive_static)
        mark_dependency_system(archive_static)
    endif()

    if(NOT TARGET woki::libarchive)
        add_library(woki_libarchive INTERFACE)
        add_library(woki::libarchive ALIAS woki_libarchive)

        if(TARGET archive)
            target_link_libraries(woki_libarchive INTERFACE archive)
        elseif(TARGET archive_static)
            target_link_libraries(woki_libarchive INTERFACE archive_static)
        elseif(LibArchive_FOUND AND LibArchive_LIBRARY AND LibArchive_INCLUDE_DIR)
            target_include_directories(woki_libarchive INTERFACE "${LibArchive_INCLUDE_DIR}")
            target_link_libraries(woki_libarchive INTERFACE "${LibArchive_LIBRARY}")
        else()
            message(FATAL_ERROR "libarchive is required on native builds but no CMake target is available")
        endif()
    endif()

    include(Wasmtime)
    woki_setup_wasmtime()

    add_custom_target(woki_thirdparty_deps)
    if(TARGET archive)
        add_dependencies(woki_thirdparty_deps archive)
    elseif(TARGET archive_static)
        add_dependencies(woki_thirdparty_deps archive_static)
    endif()
    if(TARGET wasmtime-crate)
        add_dependencies(woki_thirdparty_deps wasmtime-crate)
    endif()

    find_or_add(fmt
        NO_SYSTEM
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 10.2.1
    )
    target_compile_definitions(fmt PUBLIC FMT_CONSTEVAL=)

    find_or_add(spdlog
        NO_SYSTEM
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.1
        CMAKE_CACHE_VARS
            "SPDLOG_FMT_EXTERNAL=ON"
    )

    set(_GLFW_WAYLAND OFF)
    set(_GLFW_X11 OFF)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_GLFW_WAYLAND ON)
        set(_GLFW_X11 ON)
    endif()

    find_or_add(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG ed6452b13c76f7b4da216a9952bc7837aeb0f031
        DISABLE_INSTALL
        CMAKE_CACHE_VARS
            "GLFW_BUILD_WAYLAND=${_GLFW_WAYLAND}"
            "GLFW_BUILD_X11=${_GLFW_X11}"
    )

    find_or_add(msdfgen
        NO_SYSTEM
        GIT_REPOSITORY https://github.com/Chlumsky/msdfgen.git
        GIT_TAG v1.13
        CMAKE_CACHE_VARS
            "MSDFGEN_BUILD_STANDALONE=OFF"
            "MSDFGEN_USE_VCPKG=OFF"
            "MSDFGEN_USE_SKIA=OFF"
            "MSDFGEN_DISABLE_SVG=ON"
            "MSDFGEN_DISABLE_PNG=ON"
            "MSDFGEN_INSTALL=OFF"
    )

    set(_DAWN_VULKAN OFF)
    set(_DAWN_D3D12 OFF)
    set(_DAWN_METAL OFF)
    set(_DAWN_DESKTOP_GL OFF)
    set(_DAWN_OPENGLES OFF)
    set(_DAWN_WAYLAND OFF)
    set(_DAWN_X11 OFF)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_DAWN_VULKAN ON)
        set(_DAWN_DESKTOP_GL ON)
        set(_DAWN_OPENGLES ON)
        set(_DAWN_WAYLAND ON)
        set(_DAWN_X11 ON)
    elseif(APPLE)
        set(_DAWN_METAL ON)
    elseif(WIN32)
        set(_DAWN_D3D12 ON)
    endif()

    find_or_add(Dawn
        GIT_REPOSITORY https://github.com/google/dawn.git
        GIT_TAG 9fd4a731474f66c1ce5465f9b13919ad7487315d
        FIND_PACKAGE_ARGS CONFIG
        CMAKE_CACHE_VARS
            DAWN_FETCH_DEPENDENCIES=ON
            DAWN_BUILD_TESTS=OFF
            "DAWN_ENABLE_VULKAN=${_DAWN_VULKAN}"
            DAWN_ENABLE_D3D11=OFF
            "DAWN_ENABLE_D3D12=${_DAWN_D3D12}"
            "DAWN_ENABLE_METAL=${_DAWN_METAL}"
            DAWN_ENABLE_NULL=ON
            "DAWN_ENABLE_DESKTOP_GL=${_DAWN_DESKTOP_GL}"
            "DAWN_ENABLE_OPENGLES=${_DAWN_OPENGLES}"
            "DAWN_USE_WAYLAND=${_DAWN_WAYLAND}"
            "DAWN_USE_X11=${_DAWN_X11}"
            DAWN_USE_GLFW=ON
    )
else()
    message(STATUS "Skipping native-only dependencies for Emscripten")
endif()
