/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_BASE64_HPP
#define SNOWFLAKECLIENT_BASE64_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Snowflake
{
namespace Client
{
namespace Util
{
//TODO Move this typdef to some header files
typedef uint32_t ub4;

/**
 * Simple class used to base64-encode data.
 */
struct Base64 final
{
  /**
   * High level cpp api to encode
   * @param bytes
   * @return base64URL format code without padding
   */
  static std::string encodeURLNoPadding(const std::vector<char> &bytes);

  /**
    * High level cpp api for decode
    * @param code in base64URL format without padding
    * @return bytes
    */
  static std::vector<char> decodeURLNoPadding(const std::string &code);

  /**
   * High level cpp api to encode
   * @param bytes
   * @return base64 format code with padding
   */
  static std::string encodePadding(const std::vector<char> &bytes);

  /**
    * High level cpp api for decode
    * @param code in base64 format with padding
    * @return bytes
    */
  static std::vector<char> decodePadding(const std::string &code);


  /**
   * Provides exact amount of memory needed to store the result of encode()
   * for source of a given length.
   *
   * @note
   *    Does not include space for a trailing zero.
   */
  inline static constexpr size_t
  encodedLength(size_t srcLength) noexcept
  {
    // 4 * ceil(srcLength / 3)
    return 4 * ((srcLength + 2) / 3);
  }

  /**
   * Provides maximum amount of memory needed to store the result of decode()
   * for source of a given length. Assumes the source has been padded.
   *
   * @note
   *    Does not include space for a trailing zero.
   */
  inline static constexpr size_t
  decodedLength(size_t srcLength) noexcept
  {
    return 3 * (srcLength / 4);
  }

  /**
   * Provides exact amount of memory needed to store the result of decode()
   * for source string of a given length. Assumes the source has been padded.
   *
   * @note
   *    Does not include space for a trailing zero.
   */
  static size_t decodedLength(const void *src,
                              size_t srcLength) noexcept;

  /**
   * Encodes given source data of specified length to Base64 format.
   *
   * The 'dst' buffer needs to have sufficient memory, see encodedLength.
   * Does not add a trailing zero.
   *
   * @return
   *    Number of written bytes.
   */
  inline static size_t encode(const void *src,
                       size_t srcLength,
                       void *dst) noexcept
  {
    return encodeHelper(src, srcLength, dst, BASE64_INDEX);
  }

  /**
   * Decodes given source data of specified length with Base64 format.
   *
   * The 'dst' buffer needs to have sufficient memory, see decodedLength.
   * Does not add a trailing zero.
   *
   * @return
   *    Number of written bytes. -1 on error (invalid input).
   */
  inline static size_t decode(const void *src,
                       size_t srcLength,
                       void *dst) noexcept
  {
    return decodeHelper(src, srcLength, dst, BASE64_REV_INDEX);
  }

  /**
   * Encodes given source data of specified length to Base64url format.
   *
   * The 'dst' buffer needs to have sufficient memory, see encodedLength.
   * Does not add a trailing zero.
   *
   * @return
   *    Number of written bytes.
   */
  inline static size_t encodeUrl(const void *src,
                       size_t srcLength,
                       void *dst) noexcept
  {
    return encodeHelper(src, srcLength, dst, BASE64_URL_INDEX);
  }

  /**
   * Decodes given source data of specified length with Base64url format.
   *
   * The 'dst' buffer needs to have sufficient memory, see decodedLength.
   * Does not add a trailing zero.
   *
   * @return
   *    Number of written bytes. -1 on error (invalid input).
   */
  inline static size_t decodeUrl(const void *src,
                       size_t srcLength,
                       void *dst) noexcept
  {
    return decodeHelper(src, srcLength, dst, BASE64_URL_REV_INDEX);
  }

  // Reverse index, populated on first call.
  struct ReverseIndex
  {
    ReverseIndex(const char index_size, const char *index) noexcept
    {
        std::memset(data, 0xFF, sizeof(data));
        for (unsigned char i = 0; i < index_size; ++i)
            data[static_cast<unsigned char>(index[i])] = i;
    }

    unsigned char operator[] (int index) const
    {
        return this->data[index];
    }

    unsigned char data[256];
  };

private:
  /**
   * The RFC 4648 base64 dictionary (A-Z, a-z, 0-9, +, /).
   */
  static const char BASE64_INDEX[];

  /**
   * The RFC 4648 base64url dictionary (A-Z, a-z, 0-9, -, _).
   */
  static const char BASE64_URL_INDEX[];

  /**
   * The reverse index of the base 64 url index
   */
  static const ReverseIndex BASE64_URL_REV_INDEX;

  /**
   * The reverse index of the base 64 index
   */
  static const ReverseIndex BASE64_REV_INDEX;

   /**
   * The size of base64/base64url dictionary
   */
  static const unsigned char INDEX_SIZE;

  /**
   * Encodes given source data of specified length.
   *
   * The 'dst' buffer needs to have sufficient memory, see encodedLength.
   * Does not add a trailing zero.
   *
   * @return
   *    Number of written bytes.
   */
  static size_t encodeHelper(const void *const src, const size_t srcLength, void *const dst,
                             const char *dictionary) noexcept;

  /**
   * Decodes given source data of specified length.
   *
   * The 'dst' buffer needs to have sufficient memory, see decodedLength.
   * Does not add a trailing zero.
   *
   * @return
   *    Number of written bytes. -1 on error (invalid input).
   */
  static size_t decodeHelper(const void *const src, const size_t srcLength, void *const dst,
                             const ReverseIndex &) noexcept;

  // Not a real class. Just a namespace.
  Base64() = delete;

  ~Base64() = delete;

};
} // Util
} // Client
} // Snowflake

#endif //SNOWFLAKECLIENT_BASE64_HPP
