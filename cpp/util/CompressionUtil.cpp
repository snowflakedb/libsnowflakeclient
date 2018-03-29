/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "CompressionUtil.hpp"
#include <zlib.h>
#include <assert.h>
#include <stdio.h>

#define CHUNK 16384
#define WINDOW_BIT 15
#define GZIP_ENCODING 16

int Snowflake::Client::Util::CompressionUtil::compressWithGzip(FILE *source,
                                                               FILE *dest,
                                                               long &destSize)
{
  int ret, flush;
  unsigned have;
  z_stream strm;
  unsigned char in[CHUNK];
  unsigned char out[CHUNK];

  /* allocate deflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     WINDOW_BIT | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY);

  if (ret != Z_OK)
    return ret;

  /* compress until end of file */
  do
  {
    strm.avail_in = fread(in, 1, CHUNK, source);
    if (ferror(source))
    {
      (void) deflateEnd(&strm);
      return Z_ERRNO;
    }
    flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
    strm.next_in = in;

    /* run deflate() on input until output buffer not full, finish
       compression if all of source has been read in */
    do
    {
      strm.avail_out = CHUNK;
      strm.next_out = out;
      ret = deflate(&strm, flush);    /* no bad return value */
      assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
      have = CHUNK - strm.avail_out;
      if (fwrite(out, 1, have, dest) != have || ferror(dest))
      {
        (void) deflateEnd(&strm);
        return Z_ERRNO;
      }
    } while (strm.avail_out == 0);
    assert(strm.avail_in == 0);     /* all input will be used */

    /* done when last data in file processed */
  } while (flush != Z_FINISH);
  assert(ret == Z_STREAM_END);        /* stream will be complete */

  destSize = strm.total_out;

  /* clean up and return */
  (void) deflateEnd(&strm);
  return Z_OK;
}