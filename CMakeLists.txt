cmake_minimum_required(VERSION 3.10)
project(ThetaZ VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Source files
set(SOURCES
    main_entry.cpp
    core/loader/main.cpp
    core/ntdll/virtual.cpp
    core/ntdll/thread.cpp
    core/ntdll/sync.cpp
    core/ntdll/teb.cpp
    core/ipc/server.cpp
    core/ipc/thetaserver.cpp
    core/exception/exception.cpp
    core/exception/signal_x86_64.cpp
    core/api/api.cpp
    core/api/kernel32.cpp
    core/api/kernel32_runtime.cpp
    core/api/kernel32_winword.cpp
    core/api/memory_api.cpp
    core/api/oleaut32.cpp
    core/api/process.cpp
    core/api/ntdll_api.cpp
    core/api/mfplat.cpp
    core/api/thread_api.cpp
    core/api/ucrt.cpp
    core/api/user32.cpp
    core/api/ws2_32.cpp
    core/api/wine_compat.cpp
    core/api/kernel32_batch.cpp
    core/api/environment.cpp
    core/api/path_resolver.cpp
    core/api/window.cpp
    core/api/ole32_apis.cpp
    core/api/threadpool_runtime.cpp
    core/api/crypto_runtime.cpp
    core/x86/interp.cpp
    core/x86/api_dispatch.cpp
    core/x86/guest_env.cpp
)

# Compiler options
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -Wall -Wextra -Wno-narrowing -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Wno-address -Wno-delete-incomplete)
else()
    add_compile_options(-O2 -DNDEBUG -Wno-narrowing)
endif()

# Executable
add_executable(ThetaZ ${SOURCES})

# Library
add_library(ThetaZLib SHARED ${SOURCES})
target_link_libraries(ThetaZLib PRIVATE ${LIBFFI_LIBRARIES} ${OPENSSL_LIBRARIES} ${CURL_LIBRARIES} ssl crypto curl)

# Link libraries
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBFFI REQUIRED libffi)
pkg_check_modules(X11 REQUIRED x11)
find_package(OpenSSL REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)
target_link_libraries(ThetaZ PRIVATE Threads::Threads ${LIBFFI_LIBRARIES} ${X11_LIBRARIES} ${OPENSSL_LIBRARIES} ${CURL_LIBRARIES} ThetaZLib)
target_include_directories(ThetaZ PRIVATE ${LIBFFI_INCLUDE_DIRS} ${X11_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})

# Install rules
install(TARGETS ThetaZ DESTINATION bin)
