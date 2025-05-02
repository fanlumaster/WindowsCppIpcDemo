cmake_minimum_required(VERSION 3.25.0)
project(Win32WebviewTemplate VERSION 1.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Boost
set(Boost_ROOT "C:/Users/SonnyCalcr/scoop/apps/boost/1.87.0")
set (Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS locale)

# fmt and spdlog
set(CMAKE_PREFIX_PATH "C:/Users/SonnyCalcr/EDisk/CppCodes/Libraries/AllLibs")
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


add_definitions(
    /D_UNICODE=1
    /DUNICODE=1
)

# add_definitions(
#     /DFANY_DEBUG=1
# )

include_directories("src/")

set(SRC_FILES
    "src/common_utils.cpp"
    "src/fanylog.cpp"
    "src/main.cpp"
    "src/my_webveiw.cpp"
)

set(MY_EXECUTABLE_NAME "Win32WebviewTemplate01")

add_executable(${MY_EXECUTABLE_NAME} WIN32
    ${SRC_FILES}
)

set_target_properties(${MY_EXECUTABLE_NAME} PROPERTIES VS_GLOBAL_WebView2LoaderPreference "Static")
set_target_properties(${MY_EXECUTABLE_NAME} PROPERTIES VS_USER_PROPS "${CMAKE_SOURCE_DIR}/Demo.props")
set_target_properties(${MY_EXECUTABLE_NAME} PROPERTIES VS_PACKAGE_REFERENCES "Microsoft.Web.WebView2_1.0.3124.44;Microsoft.Windows.ImplementationLibrary_1.0.240803.1")

target_link_libraries(${MY_EXECUTABLE_NAME} fmt::fmt spdlog::spdlog Boost::locale)