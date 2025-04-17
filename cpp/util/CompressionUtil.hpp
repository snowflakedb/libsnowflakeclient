#ifndef SNOWFLAKECLIENT_COMPRESSIONUTIL_HPP
#define SNOWFLAKECLIENT_COMPRESSIONUTIL_HPP

#include <stdio.h>

namespace Snowflake
{
namespace Client
{
namespace Util
{

class CompressionUtil
{
public:
  /**
   * Compress file with gzip
   * @param source source file to compress
   * @param dest destination file that compress result will write to
   * @param destSize file size of compression result
   * @param level compression level
   * @return
   */
  static int compressWithGzip(FILE *source, FILE *dest, size_t &destSize, int level = -1);

};

}
}
}


#endif //SNOWFLAKECLIENT_COMPRESSIONUTIL_HPP
