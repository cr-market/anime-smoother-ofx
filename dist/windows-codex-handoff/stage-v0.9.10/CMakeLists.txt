cmake_minimum_required(VERSION 3.20)

if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal macOS binaries")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

project(AnimeSmootherOFX VERSION 0.9.10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckIPOSupported)
check_ipo_supported(RESULT ANIME_SMOOTHER_IPO_SUPPORTED OUTPUT ANIME_SMOOTHER_IPO_ERROR)

option(ANIME_LINE_BUILD_STANDALONE_TEST "Build the standalone core test" ON)

set(OFX_SUPPORT_ROOT "" CACHE PATH "Path to an OpenFX Support Library checkout")

set(LOILO_SMOOTH_SOURCES
    external/smooth/util.cpp
    external/smooth/upMode.cpp
    external/smooth/downMode.cpp
    external/smooth/8link.cpp
    external/smooth/Lack.cpp
)

if(OFX_SUPPORT_ROOT)
    add_library(OpenFXSupport STATIC
        "${OFX_SUPPORT_ROOT}/Support/Library/ofxsCore.cpp"
        "${OFX_SUPPORT_ROOT}/Support/Library/ofxsImageEffect.cpp"
        "${OFX_SUPPORT_ROOT}/Support/Library/ofxsInteract.cpp"
        "${OFX_SUPPORT_ROOT}/Support/Library/ofxsLog.cpp"
        "${OFX_SUPPORT_ROOT}/Support/Library/ofxsMultiThread.cpp"
        "${OFX_SUPPORT_ROOT}/Support/Library/ofxsParams.cpp"
        "${OFX_SUPPORT_ROOT}/Support/Library/ofxsProperty.cpp"
        "${OFX_SUPPORT_ROOT}/Support/Library/ofxsPropertyValidation.cpp"
    )

    target_include_directories(OpenFXSupport PUBLIC
        "${OFX_SUPPORT_ROOT}/include"
        "${OFX_SUPPORT_ROOT}/Support/include"
    )

    target_compile_features(OpenFXSupport PUBLIC cxx_std_17)

    add_library(AnimeSmoother.ofx MODULE
        src/MLAAEdgeSmoother.cpp
        ${LOILO_SMOOTH_SOURCES}
    )

    target_compile_features(AnimeSmoother.ofx PRIVATE cxx_std_17)
    target_compile_options(AnimeSmoother.ofx PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-O3>
        $<$<CXX_COMPILER_ID:MSVC>:/O2>
    )
    target_include_directories(AnimeSmoother.ofx PRIVATE
        "${OFX_SUPPORT_ROOT}/include"
        "${OFX_SUPPORT_ROOT}/Support/include"
        "${OFX_SUPPORT_ROOT}/Support/Plugins/include"
        src
        src/ae_compat
        external/smooth
    )
    target_link_libraries(AnimeSmoother.ofx PRIVATE OpenFXSupport)
    if(ANIME_SMOOTHER_IPO_SUPPORTED)
        set_property(TARGET AnimeSmoother.ofx PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endif()

    if(APPLE)
        set_target_properties(AnimeSmoother.ofx PROPERTIES
            BUNDLE TRUE
            BUNDLE_EXTENSION "ofx.bundle"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/OFXBundleInfo.plist.in"
            PREFIX ""
            OUTPUT_NAME "AnimeSmoother"
        )
        add_custom_command(TARGET AnimeSmoother.ofx POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E rm -f "$<TARGET_BUNDLE_DIR:AnimeSmoother.ofx>/Contents/MacOS/AnimeSmoother.ofx"
            COMMAND "${CMAKE_COMMAND}" -E rename "$<TARGET_BUNDLE_DIR:AnimeSmoother.ofx>/Contents/MacOS/AnimeSmoother" "$<TARGET_BUNDLE_DIR:AnimeSmoother.ofx>/Contents/MacOS/AnimeSmoother.ofx"
        )
    elseif(WIN32)
        set_target_properties(AnimeSmoother.ofx PROPERTIES
            PREFIX ""
            SUFFIX ".ofx"
            OUTPUT_NAME "AnimeSmoother"
            WINDOWS_EXPORT_ALL_SYMBOLS ON
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        )
        add_custom_command(TARGET AnimeSmoother.ofx POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/ofx-package/AnimeSmoother.ofx.bundle/Contents/Win64"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:AnimeSmoother.ofx>" "${CMAKE_BINARY_DIR}/ofx-package/AnimeSmoother.ofx.bundle/Contents/Win64/AnimeSmoother.ofx"
        )
    else()
        set_target_properties(AnimeSmoother.ofx PROPERTIES
            PREFIX ""
            SUFFIX ".ofx"
            OUTPUT_NAME "AnimeSmoother"
        )
    endif()
else()
    message(STATUS "OFX_SUPPORT_ROOT is not set; only the standalone MLAA core test will be built.")
endif()

if(ANIME_LINE_BUILD_STANDALONE_TEST)
    add_executable(mlaa_core_test tests/mlaa_core_test.cpp ${LOILO_SMOOTH_SOURCES})
    target_include_directories(mlaa_core_test PRIVATE src src/ae_compat external/smooth)
    target_compile_features(mlaa_core_test PRIVATE cxx_std_17)
    target_compile_options(mlaa_core_test PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-O3>
        $<$<CXX_COMPILER_ID:MSVC>:/O2>
    )
    if(ANIME_SMOOTHER_IPO_SUPPORTED)
        set_property(TARGET mlaa_core_test PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endif()
endif()
