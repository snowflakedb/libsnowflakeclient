//
// Created by hyu on 3/26/18.
//

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
   * @return
   */
  static int compressWithGzip(FILE *source, FILE *dest, long &destSize);
};

}
}
}


#endif //SNOWFLAKECLIENT_COMPRESSIONUTIL_HPP
