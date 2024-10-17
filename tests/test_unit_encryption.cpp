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

void log_base_64_value(const char *input, const int input_len, const std::string& message) {
    auto input_base64 = std::unique_ptr<char[]>(new char[128]);
    size_t encoded_len = Snowflake::Client::Util::Base64::encode(input, input_len, input_base64.get());
    input_base64[encoded_len] = '\0';
    printf("%s Base 64 %s", message.c_str(), input_base64.get());
}

std::unique_ptr<char[]> concat_and_encode_base64(char* input1, int input1_len, char* input2, int input2_len) {
    const auto concatenated_input = std::unique_ptr<char[]>(new char[input1_len + input2_len + 1]);
    auto encoded_str = std::unique_ptr<char[]>(new char[128]);
    memcpy(concatenated_input.get(), input1, input1_len);
    memcpy(concatenated_input.get() + input1_len, input2, input2_len);
    size_t encoded_len = Snowflake::Client::Util::Base64::encode(concatenated_input.get(), input1_len + input2_len, encoded_str.get());
    encoded_str[encoded_len] = '\0';

    return encoded_str;
}

CryptoKey create_key(const std::string& key_str, int nbBits) {
    CryptoKey key;
    key.nbBits = nbBits;
    key_str.copy(key.data, nbBits/8);

    return key;
}

CryptoIV create_iv(const std::string& iv_str) {
    CryptoIV iv;
    iv_str.copy(iv.data, SF_CRYPTO_IV_NBITS/8);

    return iv;
}

/**
 * Test AES CBC encryption
 */
void test_aes_cbc_mode_encryption(void **unused) {
    const std::string plaintext = "abc";
    // pragma: allowlist nextline secret
    const CryptoKey key = create_key("1234567890abcdef", 128);
    // pragma: allowlist nextline secret
    const CryptoIV iv = create_iv("abcdef1234567890");

    CipherContext cipher_context = Cryptor::getInstance().createCipherContext(
        CryptoAlgo::AES,
        CryptoMode::CBC, CryptoPadding::PKCS5, key, iv, false);
    cipher_context.initialize(CryptoOperation::ENCRYPT);

    const auto ciphertext = std::unique_ptr<char[]>(new char[128]);
    size_t next_size = cipher_context.next(ciphertext.get(), plaintext.c_str(), plaintext.length());
    size_t len = cipher_context.finalize(ciphertext.get() + next_size);
    ciphertext[len] = '\0';

    cipher_context.initialize(CryptoOperation::DECRYPT);
    auto plaintext_decrypted = std::unique_ptr<char[]>(new char[128]);
    next_size = cipher_context.next(plaintext_decrypted.get(), ciphertext.get(), len);
    len = cipher_context.finalize(plaintext_decrypted.get() + next_size);
    plaintext_decrypted[len] = '\0';

    assert_string_equal(plaintext_decrypted.get(), plaintext.c_str());
}

void test_aes_gcm_mode_encryption(void **unused) {
    const std::string plaintext = "abc";
    // pragma: allowlist nextline secret
    const CryptoKey key = create_key("1234567890abcdef", 128);
    // pragma: allowlist nextline secret
    const CryptoIV iv = create_iv("abcdef1234567890");
    const std::string expected_ciphertext_base64 = "pgs/wjNH2TYekmN7mbhFjeHH0A==";

    CipherContext cipher_context = Cryptor::getInstance().createCipherContext(
        CryptoAlgo::AES,
        CryptoMode::GCM, CryptoPadding::PKCS5, key, iv, false);
    cipher_context.initialize(CryptoOperation::ENCRYPT);

    const auto ciphertext = std::unique_ptr<char[]>(new char[128]);
    auto tag = std::unique_ptr<unsigned char[]>(new unsigned char[16]);

    size_t next_size = cipher_context.next(ciphertext.get(), plaintext.c_str(), plaintext.length());
    size_t len = cipher_context.finalize(ciphertext.get() + next_size, tag.get());
    ciphertext[len] = '\0';
    auto ciphertext_base64 = std::unique_ptr<char[]>(new char[128]);

    cipher_context.initialize(CryptoOperation::DECRYPT);
    auto plaintext_decrypted = std::unique_ptr<char[]>(new char[128]);
    next_size = cipher_context.next(plaintext_decrypted.get(), ciphertext.get(), len);
    len = cipher_context.finalize(plaintext_decrypted.get() + next_size, tag.get());
    plaintext_decrypted[len] = '\0';

    assert_string_equal(plaintext_decrypted.get(), plaintext.c_str());
    assert_string_equal(concat_and_encode_base64(ciphertext.get(), len, reinterpret_cast<char *>(tag.get()), 16).get(), expected_ciphertext_base64.c_str());
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_aes_cbc_mode_encryption),
        cmocka_unit_test(test_aes_gcm_mode_encryption),
    };
    const int ret = cmocka_run_group_tests(tests, nullptr, nullptr);
    return ret;
}
