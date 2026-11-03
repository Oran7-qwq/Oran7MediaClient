add_executable(pure_checks main.cpp
    "${ORAN7_SOURCE_ROOT}/src/cpp/GlobalSingleton/Oran7LrcParser.cpp"
    "${ORAN7_SOURCE_ROOT}/src/ncm/crypto/CryptoProvider.cpp"
    "${ORAN7_SOURCE_ROOT}/src/ncm/crypto/WeapiCrypto.cpp")
target_include_directories(pure_checks PRIVATE
    "${ORAN7_SOURCE_ROOT}/src/cpp/GlobalSingleton" "${ORAN7_SOURCE_ROOT}/src/ncm/crypto")
set_source_files_properties(
    "${ORAN7_SOURCE_ROOT}/src/ncm/crypto/CryptoProvider.cpp"
    "${ORAN7_SOURCE_ROOT}/src/ncm/crypto/WeapiCrypto.cpp"
    PROPERTIES INCLUDE_DIRECTORIES "${OPENSSL_ROOT_DIR}/include")
target_link_libraries(pure_checks PRIVATE Oran7Core Qt6::Core "${ORAN7_CRYPTO_LIBRARY}"
    "${ORAN7_WINPTHREAD_LIBRARY}" "${ORAN7_SSP_LIBRARY}")
