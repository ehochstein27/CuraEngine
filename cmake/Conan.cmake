macro(run_conan)
    # Download automatically, you can also just copy the conan.cmake file
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
        message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
        file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v0.15/conan.cmake" "${CMAKE_BINARY_DIR}/conan.cmake")
    endif()

    include(${CMAKE_BINARY_DIR}/conan.cmake)

    conan_add_remote(
            NAME
            bincrafters
            URL
            https://api.bintray.com/conan/bincrafters/public-conan)

    set(CONAN_PROFILE "" CACHE STRING "Conan profile to use (i.e. conan's --profile option")

    conan_cmake_run(
            REQUIRES
            stb/20200203
            clipper/6.4.2
            OPTIONS
            ${CONAN_EXTRA_OPTIONS}
            BASIC_SETUP
            CMAKE_TARGETS
            GENERATORS cmake_find_package
            BUILD
            missing
            PROFILE ${CONAN_PROFILE}
            SETTINGS compiler.cppstd=11)

    list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
    add_compile_definitions(STB_CONAN)
endmacro()
