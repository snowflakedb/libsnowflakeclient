/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <iostream>

#include <ostream>

#include "utils/test_setup.h"
#include "crypto/CipherContext.hpp"
#include "crypto/Cryptor.hpp"
#include "logger/SFLogger.hpp"
#include "util/Base64.hpp"

using namespace Snowflake::Client::Crypto;
/**
 * Test AES CBC encryption
 */
void test_aes_cbc_mode_encryption(void **unused) {
    CryptoRandomDevice randomDevice = CryptoRandomDevice::DEV_URANDOM;

    CryptoKey key;
    Cryptor::generateKey(key, 128, randomDevice);
    CryptoIV iv;
    Cryptor::generateIV(iv, randomDevice);
    CipherContext cipher_context = Cryptor::getInstance().createCipherContext(
        CryptoAlgo::AES,
        CryptoMode::CBC, CryptoPadding::PKCS5, key, iv, false);
    std::string plaintext = "abcdefgh";
    const auto ciphertext = std::unique_ptr<char[]>(new char[128]);

    cipher_context.initialize(CryptoOperation::ENCRYPT);
    size_t next_size = cipher_context.next(ciphertext.get(), plaintext.c_str(), plaintext.length());
    size_t len = cipher_context.finalize(ciphertext.get() + next_size);
    auto ciphertext_base64 = std::unique_ptr<char[]>(new char[128]);
    size_t encoded_len = Snowflake::Client::Util::Base64::encode(ciphertext.get(), len, ciphertext_base64.get());
    ciphertext_base64[encoded_len] = '\0';
    std::cout << "ciphertext Base64: " << ciphertext_base64 << std::endl;

    auto plaintext_decrypted = std::unique_ptr<char[]>(new char[128]);
    cipher_context.initialize(CryptoOperation::DECRYPT);
    next_size = cipher_context.next(plaintext_decrypted.get(), ciphertext.get(), len);
    std::cout << "next_size: " << next_size << std::endl;
    len = cipher_context.finalize(plaintext_decrypted.get() + next_size);
    plaintext_decrypted[len] = '\0';
    std::cout << "plaintext_decrypted " << plaintext_decrypted.get() << std::endl;
    std::cout << "len " << len << std::endl;

    assert_string_equal(plaintext_decrypted.get(), plaintext.c_str());
}

void log_base_64_value(char *input, int input_len, std::string message) {
    auto input_base64 = std::unique_ptr<char[]>(new char[128]);
    size_t encoded_len = Snowflake::Client::Util::Base64::encode(input, input_len, input_base64.get());
    input_base64[encoded_len] = '\0';
    printf("%s Base 64 %s", message.c_str(), input_base64.get());
}


void test_aes_gcm_mode_encryption(void **unused) {
    CryptoRandomDevice randomDevice = CryptoRandomDevice::DEV_URANDOM;

    CryptoKey key;
    Cryptor::generateKey(key, 128, randomDevice);
    CryptoIV iv;
    Cryptor::generateIV(iv, randomDevice);
    CipherContext cipher_context = Cryptor::getInstance().createCipherContext(
        CryptoAlgo::AES,
        CryptoMode::GCM, CryptoPadding::PKCS5, key, iv, false);
    std::string plaintext = "abcdefgh";
    const auto ciphertext = std::unique_ptr<char[]>(new char[128]);

    unsigned char tag[16] = "abcdefghijklmno";

    cipher_context.initialize(CryptoOperation::ENCRYPT);
    size_t next_size = cipher_context.next(ciphertext.get(), plaintext.c_str(), plaintext.length());
    size_t len = cipher_context.finalize(ciphertext.get() + next_size, tag);
    auto ciphertext_base64 = std::unique_ptr<char[]>(new char[128]);
    size_t encoded_len = Snowflake::Client::Util::Base64::encode(ciphertext.get(), len, ciphertext_base64.get());
    ciphertext_base64[encoded_len] = '\0';
    std::cout << "ciphertext Base64: " << ciphertext_base64 << std::endl;

    auto plaintext_decrypted = std::unique_ptr<char[]>(new char[128]);
    cipher_context.initialize(CryptoOperation::DECRYPT);
    next_size = cipher_context.next(plaintext_decrypted.get(), ciphertext.get(), len);
    std::cout << "next_size: " << next_size << std::endl;
    len = cipher_context.finalize(plaintext_decrypted.get() + next_size, tag);
    plaintext_decrypted[len] = '\0';
    std::cout << "plaintext_decrypted " << plaintext_decrypted.get() << std::endl;
    std::cout << "len " << len << std::endl;

    assert_string_equal(plaintext_decrypted.get(), plaintext.c_str());
}

void test_base64(void **unused) {
    std::string data = "Hello World!";
    char *data_base64 = new char();
    size_t base64_size = Snowflake::Client::Util::Base64::encode(data.c_str(), data.length(), data_base64);
    std::cout << "data Base64: " << data_base64 << std::endl;

    // char data_decoded[data.length()];
    auto data_decoded = std::unique_ptr<char[]>(new char[128]);
    size_t res = Snowflake::Client::Util::Base64::decode(data_base64, base64_size, data_decoded.get());
    std::cout << "res " << res << std::endl;

    data_decoded[res] = '\0';

    assert_string_equal(data_decoded.get(), data.c_str());
}

int main() {
    const struct CMUnitTest tests[] = {
        // cmocka_unit_test(test_aes_cbc_mode_encryption),
        cmocka_unit_test(test_aes_gcm_mode_encryption),
        // cmocka_unit_test(test_base64)
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    return ret;
}
