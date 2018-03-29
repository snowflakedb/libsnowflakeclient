//
// Created by Haowei Yu on 2/26/18.
//

#include "Base64.hpp"
#include <cstring>

namespace Snowflake
{
namespace Client
{
namespace Util
{
const char Base64::BASE64_INDEX[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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

size_t Base64::encode(const void *const vsrc,
                      const size_t srcLength,
                      void *const vdst) noexcept
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

    dst[j++] = BASE64_INDEX[(combined >> 18) & 0x3F];
    dst[j++] = BASE64_INDEX[(combined >> 12) & 0x3F];
    dst[j++] = BASE64_INDEX[(combined >> 6) & 0x3F];
    dst[j++] = BASE64_INDEX[(combined >> 0) & 0x3F];
  }

  // Add trailing "=" or "==" for inputs not being a multiple of 3.
  if (srcLength % 3 >= 1)
    dst[j - 1] = '=';
  if (srcLength % 3 == 1)
    dst[j - 2] = '=';

  return j;
}

size_t Base64::decode(const void *const vsrc,
                      const size_t srcLength,
                      void *const vdst) noexcept
{
  // Reverse index, populated on first call.
  struct ReverseIndex final
  {
    ReverseIndex()
    {
      std::memset(data, 0xFF, sizeof(data));
      for (size_t i = 0; i < sizeof(BASE64_INDEX); ++i)
        data[static_cast<unsigned char>(BASE64_INDEX[i])] = i;
    }

    unsigned char data[256];
  } static const BASE64_REV_INDEX;

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
      const ub4 b1 = BASE64_REV_INDEX.data[src[i]];
      const ub4 b2 = BASE64_REV_INDEX.data[src[i + 1]];
      const ub4 b3 = BASE64_REV_INDEX.data[src[i + 2]];
      const ub4 b4 = BASE64_REV_INDEX.data[src[i + 3]];

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
      const ub4 b1 = BASE64_REV_INDEX.data[src[i]];
      const ub4 b2 = BASE64_REV_INDEX.data[src[i + 1]];

      // Check for illegal input.
      if ((b1 == 0xFF) || (b2 == 0xFF) || (src[i + 3] != '='))
        return -1;

      const ub4 combined = (b1 << 18) | (b2 << 12);
      dst[j++] = combined >> 16;
    } else if (src[i + 3] == '=')
    {
      // Last block contains two bytes.
      const ub4 b1 = BASE64_REV_INDEX.data[src[i]];
      const ub4 b2 = BASE64_REV_INDEX.data[src[i + 1]];
      const ub4 b3 = BASE64_REV_INDEX.data[src[i + 2]];

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
}
}
}