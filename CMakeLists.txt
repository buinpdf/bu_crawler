cmake_minimum_required(VERSION 3.25)

project(bu_crawler)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto SSL)

add_executable (${PROJECT_NAME}
    main.cpp
    root_certificates.hpp
    lex.cpp
    lex.h
    dates_conv.h
    dates_conv.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${Boost_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
