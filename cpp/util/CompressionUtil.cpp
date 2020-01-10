/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "CompressionUtil.hpp"
#include <zlib.h>
#include <assert.h>
#include <stdio.h>

#define CHUNK 16384
#define WINDOW_BIT 15
#define GZIP_ENCODING 16

#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) _setmode(_fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif


int Snowflake::Client::Util::CompressionUtil::compressWithGzip(FILE *source,
                                                               FILE *dest,
                                                               long &destSize)
{
  SET_BINARY_MODE(source);
  SET_BINARY_MODE(dest);

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
    strm.avail_in = (unsigned int)fread(in, 1, CHUNK, source);
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

/* Decompress from file source to file dest until stream ends or EOF.
inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
allocated for processing, Z_DATA_ERROR if the deflate data is
invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
the version of the library linked do not match, or Z_ERRNO if there
is an error reading or writing the files. */
int Snowflake::Client::Util::CompressionUtil::decompressWithGzip(FILE *source,
  FILE *dest)
{
  int ret;
  unsigned have;
  z_stream strm;
  unsigned char in[CHUNK];
  unsigned char out[CHUNK];

  /* allocate inflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  ret = inflateInit2(&strm, WINDOW_BIT | 32);
  if (ret != Z_OK)
    return ret;

  /* decompress until deflate stream ends or end of file */
  do {
    strm.avail_in = (unsigned int)fread(in, 1, CHUNK, source);
    if (ferror(source)) {
      (void)inflateEnd(&strm);
      return Z_ERRNO;
    }
    if (strm.avail_in == 0)
      break;
    strm.next_in = in;

    /* run inflate() on input until output buffer not full */
    do {
      strm.avail_out = CHUNK;
      strm.next_out = out;
      ret = inflate(&strm, Z_NO_FLUSH);
      assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
      switch (ret) {
      case Z_NEED_DICT:
        ret = Z_DATA_ERROR;     /* and fall through */
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
        (void)inflateEnd(&strm);
        return ret;
      }
      have = CHUNK - strm.avail_out;
      if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
        (void)inflateEnd(&strm);
        return Z_ERRNO;
      }
    } while (strm.avail_out == 0);

    /* done when inflate() says it's done */
  } while (ret != Z_STREAM_END);

  /* clean up and return */
  (void)inflateEnd(&strm);
  return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}
