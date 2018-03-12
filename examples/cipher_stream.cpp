//
// Created by Haowei Yu on 3/9/18.
//

#include "crypto/CryptoTypes.hpp"
#include "crypto/CipherContext.hpp"
#include "crypto/Cryptor.hpp"
#include "crypto/CipherStream.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <cassert>

using namespace Snowflake::Client::Crypto;


int main()
{
    CryptoKey key;

    CryptoIV iv;

    Cryptor::generateKey(key, 128, CryptoRandomDevice::DEV_RANDOM);

    Cryptor::generateIV(iv, CryptoRandomDevice::DEV_RANDOM);

    std::string input_str = "1234556677883y14724678236478236478267846278467824678278";

    std::basic_stringstream<char> in_ss(input_str);

    CipherStream encStream(in_ss, CryptoOperation::ENCRYPT, key, iv);
    CipherStream decStream(encStream, CryptoOperation::DECRYPT, key, iv);

    char res[100];
    decStream.read(res, 100);
    assert(strcmp(input_str.c_str(), res) == 0);
}
