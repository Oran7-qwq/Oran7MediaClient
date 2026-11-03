add_executable(credentials_checks main.cpp
    "${ORAN7_SOURCE_ROOT}/src/ncm/network/CookieStore.cpp"
    "${ORAN7_SOURCE_ROOT}/src/ncm/network/CookieStore.h")
set_target_properties(credentials_checks PROPERTIES AUTOMOC ON)
target_include_directories(credentials_checks PRIVATE
    "${ORAN7_SOURCE_ROOT}/src/cpp/GlobalSingleton"
    "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7Config"
    "${ORAN7_SOURCE_ROOT}/src/ncm/network")
target_compile_definitions(credentials_checks PRIVATE ORAN7_TEST_SOURCE_ROOT="${ORAN7_SOURCE_ROOT}")
target_link_libraries(credentials_checks PRIVATE Oran7Core Qt6::Core Qt6::Network crypt32)
