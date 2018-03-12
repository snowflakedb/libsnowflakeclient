//
// Created by Haowei Yu on 2/26/18.
//

#ifndef SNOWFLAKECLIENT_BASE64_HPP
#define SNOWFLAKECLIENT_BASE64_HPP

#include <cstddef>
#include <cstdint>

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
            struct Base64 final {

                /**
                 * Provides exact amount of memory needed to store the result of encode()
                 * for source of a given length.
                 *
                 * @note
                 *    Does not include space for a trailing zero.
                 */
                static constexpr size_t
                encodedLength(size_t srcLength) noexcept {
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
                static constexpr size_t
                decodedLength(size_t srcLength) noexcept {
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
                 * Encodes given source data of specified length.
                 *
                 * The 'dst' buffer needs to have sufficient memory, see encodedLength.
                 * Does not add a trailing zero.
                 *
                 * @return
                 *    Number of written bytes.
                 */
                static size_t encode(const void *src,
                                     size_t srcLength,
                                     void *dst) noexcept;

                /**
                 * Decodes given source data of specified length.
                 *
                 * The 'dst' buffer needs to have sufficient memory, see decodedLength.
                 * Does not add a trailing zero.
                 *
                 * @return
                 *    Number of written bytes. -1 on error (invalid input).
                 */
                static size_t decode(const void *src,
                                     size_t srcLength,
                                     void *dst) noexcept;

            private:
                /**
                 * The RFC 4648 base64 dictionary (A-Z, a-z, 0-9, +, /).
                 */
                static const char BASE64_INDEX[];

                // Not a real class. Just a namespace.
                Base64() = delete;

                ~Base64() = delete;

            };
        } // Util
    } // Client
} // Snowflake

#endif //SNOWFLAKECLIENT_BASE64_HPP
