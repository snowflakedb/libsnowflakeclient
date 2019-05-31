/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "Base64.hpp"
#include <cstring>
#include "snowflake/IBase64.hpp"
#include "../logger/SFLogger.hpp"

namespace Snowflake
{
namespace Client
{
namespace Util
{
const char Base64::BASE64_INDEX[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const char Base64::BASE64_URL_INDEX[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

const unsigned char Base64::INDEX_SIZE = 64;

const Base64::ReverseIndex
Base64::BASE64_REV_INDEX{64, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

const Base64::ReverseIndex
Base64::BASE64_URL_REV_INDEX{64, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"};

std::string Base64::encodeURLNoPadding(const std::vector<char> &bytes)
{
  size_t buf_len = encodedLength(bytes.size());
  std::string buffer(buf_len, 0);
  size_t len = encodeUrl(bytes.data(), bytes.size(), (void *) buffer.data());

  // remove the end of padding
  while (buffer[len - 1] == '=') len--;

  return buffer.substr(0, len);
}

std::vector<char> Base64::decodeURLNoPadding(const std::string &text)
{
  // add padding to the end
  size_t pad_len = (4 - text.length() % 4) % 4;
  std::string in_text = text + std::string(pad_len, '=');

  // Base 64 decode text
  size_t decode_len = Client::Util::Base64::decodedLength(in_text.length());
  std::vector<char> decoded(decode_len);
  decode_len = decodeUrl(in_text.c_str(), in_text.length(), decoded.data());

  if (decode_len == static_cast<size_t >(-1L))
  {
    CXX_LOG_DEBUG("Fail to decode the string: %s", text.c_str());
    throw Base64DecodeException("Decode of base64URL with no padding failed");
  }

  decoded.resize(decode_len);
  return decoded;
}

std::string Base64::encodePadding(const std::vector<char> &bytes)
{
  size_t buf_len = encodedLength(bytes.size());
  std::string buffer(buf_len, 0);
  size_t len = encode(bytes.data(), bytes.size(), (void *) buffer.data());

  return buffer.substr(0, len);
}

std::vector<char> Base64::decodePadding(const std::string &text)
{
  // Base 64 decode text
  size_t decode_len = Client::Util::Base64::decodedLength(text.length());
  std::vector<char> decoded(decode_len);
  decode_len = decode(text.c_str(), text.length(), decoded.data());

  if (decode_len == static_cast<size_t >(-1L))
  {
    CXX_LOG_DEBUG("Fail to decode the string: %s", text.c_str());
    throw Base64DecodeException("decode of base64 with padding failed");
  }

  decoded.resize(decode_len);
  return decoded;
}

size_t Base64::decodedLength(const void *const vsrc,
                             const size_t srcLength) noexcept
{
  if (!srcLength)
  {
    // Empty string.
    return 0;
  } else if (!(srcLength % 4))
  {
    // Non-empty string. Check for padding bytes.
    const char *const src = static_cast<const char *>(vsrc);
    size_t padBytes(0);
    if (src[srcLength - 2] == '=')
      padBytes = 2;
    else if (src[srcLength - 1] == '=')
      padBytes = 1;
    else
      padBytes = 0;
    return 3 * (srcLength / 4) - padBytes;
  } else
  {
    // Invalid input length. Padding error.
    return -1;
  }
}

size_t Base64::encodeHelper(const void *const vsrc,
                            const size_t srcLength,
                            void *const vdst,
                            const char *dictionary) noexcept
{
  const unsigned char *const src = static_cast<const unsigned char *>(vsrc);
  char *const dst = static_cast<char *>(vdst);

  // Encode 3 input bytes to 4 output bytes at a time.
  size_t j = 0;
  for (size_t i = 0; i < srcLength;)
  {
    const ub4 b1 = src[i++];
    const ub4 b2 = i < srcLength ? src[i++] : 0;
    const ub4 b3 = i < srcLength ? src[i++] : 0;
    const ub4 combined = (b1 << 16) | (b2 << 8) | b3;

    dst[j++] = dictionary[(combined >> 18) & 0x3F];
    dst[j++] = dictionary[(combined >> 12) & 0x3F];
    dst[j++] = dictionary[(combined >> 6) & 0x3F];
    dst[j++] = dictionary[(combined >> 0) & 0x3F];
  }

  // Add trailing "=" or "==" for inputs not being a multiple of 3.
  if (srcLength % 3 >= 1)
    dst[j - 1] = '=';
  if (srcLength % 3 == 1)
    dst[j - 2] = '=';

  return j;
}

size_t Base64::decodeHelper(const void *const vsrc,
                            const size_t srcLength,
                            void *const vdst,
                            const ReverseIndex &REV_INDEX) noexcept
{
  // Check for correct padding.
  if (srcLength % 4)
    return -1;

  const unsigned char *const src = static_cast<const unsigned char *>(vsrc);
  char *const dst = static_cast<char *>(vdst);

  // Decode unpadded data.
  size_t i = 0, j = 0;
  if (srcLength > 4)
  {
    while (i < (srcLength - 4))
    {
      no_padding:
      const ub4 b1 = REV_INDEX[src[i]];
      const ub4 b2 = REV_INDEX[src[i + 1]];
      const ub4 b3 = REV_INDEX[src[i + 2]];
      const ub4 b4 = REV_INDEX[src[i + 3]];

      // Check for illegal input.
      if ((b1 == 0xFF) || (b2 == 0xFF) || (b3 == 0xFF) ||
          (b4 == 0xFF))
        return -1;

      const ub4 combined = (b1 << 18) | (b2 << 12) | (b3 << 6) | b4;
      dst[j++] = combined >> 16;
      dst[j++] = combined >> 8;
      dst[j++] = combined;

      i += 4;
    }
  }

  // Decode (potentially) padded data.
  if (i < srcLength)
  {
    if (src[i + 2] == '=')
    {
      // Last block contains only one byte.
      const ub4 b1 = REV_INDEX[src[i]];
      const ub4 b2 = REV_INDEX[src[i + 1]];

      // Check for illegal input.
      if ((b1 == 0xFF) || (b2 == 0xFF) || (src[i + 3] != '='))
        return -1;

      const ub4 combined = (b1 << 18) | (b2 << 12);
      dst[j++] = combined >> 16;
    } else if (src[i + 3] == '=')
    {
      // Last block contains two bytes.
      const ub4 b1 = REV_INDEX[src[i]];
      const ub4 b2 = REV_INDEX[src[i + 1]];
      const ub4 b3 = REV_INDEX[src[i + 2]];

      // Check for illegal input.
      if ((b1 == 0xFF) || (b2 == 0xFF) || (b3 == 0xFF))
        return -1;

      const ub4 combined = (b1 << 18) | (b2 << 12) | (b3 << 6);
      dst[j++] = combined >> 16;
      dst[j++] = combined >> 8;
    } else
    {
      // Last block contains three bytes.
      goto no_padding;
    }
  }

  return j;
}

std::string IBase64::encodeURLNoPadding(const std::vector<char> &bytes)
{
  return Base64::encodeURLNoPadding(bytes);
}

std::string IBase64::encodePadding(const std::vector<char> &bytes)
{
  return Base64::encodePadding(bytes);
}

std::vector<char> IBase64::decodeURLNoPadding(const std::string &code)
{
  return Base64::decodeURLNoPadding(code);
}

std::vector<char> IBase64::decodePadding(const std::string &code)
{
  return Base64::decodePadding(code);
}

}
}
}