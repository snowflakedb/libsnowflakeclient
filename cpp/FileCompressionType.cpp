/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <fstream>
#include <ios>
#include <cstring>
#include <snowflake/platform.h>
#include "FileCompressionType.hpp"

using Snowflake::Client::FileCompressionType;

#define MAX_MAGIC_BYTES 4

#define GZIP_MAGIC "\x1f\x8b"
#define GZIP_NAME "gzip"
#define GZIP_EXTENSION ".gz"
#define DEFLATE_MAGIC_LOW "\x78\x01"
#define DEFLATE_MAGIC_DEFAULT "\x78\x9c"
#define DEFLATE_MAGIC_BEST "\x78\xda"
#define DEFLATE_NAME "deflate"
#define DEFLATE_EXTENSION ".deflate"
#define RAW_DEFLATE_NAME "raw_deflate"
#define RAW_DEFLATE_EXTENSION ".raw_deflate"
#define BZIP2_MAGIC "\x42\x5a"
#define BZIP2_NAME "bzip2"
#define BZIP2_EXTENSION ".bz2"
#define ZSTD_MAGIC "\x28\xb5\x2f\xfd"
#define ZSTD_NAME "zstd"
#define ZSTD_EXTENSION ".zst"
#define BROTLI_MAGIC "\xce\xb2\xcf\x81"
#define BROTLI_NAME "brotli"
#define BROTLI_EXTENSION ".br"
#define LZIP_NAME "lzip"
#define LZIP_EXTENSION ".lz"
#define LZMA_NAME "lzma"
#define LZMA_EXTENSION ".lzma"
#define LZO_NAME "lzop"
#define LZO_EXTENSION ".lzo"
#define XZ_NAME "xz"
#define XZ_EXTENSION ".xz"
#define COMPRESS_NAME "compress"
#define COMPRESS_EXTENSION ".Z"
#define PARQUET_MAGIC "\x50\x41\x52\x31"
#define PARQUET_NAME "parquet"
#define PARQUET_EXTENSION ".parquet"
#define ORC_MAGIC "\x4f\x52\x43"
#define ORC_NAME "orc"
#define ORC_EXTENSION ".orc"
#define NONE_NAME "none"
#define NONE_EXTENSION ""

std::vector<const char *> gzip_magics = {GZIP_MAGIC};
std::vector<const char *> deflate_magics = {DEFLATE_MAGIC_LOW,
                                            DEFLATE_MAGIC_DEFAULT,
                                            DEFLATE_MAGIC_BEST};
std::vector<const char *> bzip2_magics = {BZIP2_MAGIC};
std::vector<const char *> orc_magics = {ORC_MAGIC};
std::vector<const char *> parquet_magics = {PARQUET_MAGIC};
std::vector<const char *> zstd_magics = {ZSTD_MAGIC};
std::vector<const char *> brotli_magics = {BROTLI_MAGIC};

Snowflake::Client::FileCompressionType::FileCompressionType(
  const char * fileExtension,
  const char * name,
  bool isSupported) :
  Snowflake::Client::FileCompressionType(fileExtension, name, {}, 0, isSupported)
{
}

Snowflake::Client::FileCompressionType::FileCompressionType(
  const char * fileExtension,
  const char * name,
  std::vector<const char *> *magicNumbers,
  short magicBytes,
  bool isSupported){
  m_fileExtension = fileExtension;
  m_isSupported = isSupported;
  m_magicNumbers = magicNumbers;
  m_magicBytes = magicBytes;
  m_name = name;
}

const std::vector<const FileCompressionType *> FileCompressionType::types = {
  &FileCompressionType::GZIP,
  &FileCompressionType::DEFLATE,
  &FileCompressionType::RAW_DEFLATE,
  &FileCompressionType::BZIP2,
  &FileCompressionType::ZSTD,
  &FileCompressionType::BROTLI,
  &FileCompressionType::LZIP,
  &FileCompressionType::LZMA,
  &FileCompressionType::LZO,
  &FileCompressionType::XZ,
  &FileCompressionType::COMPRESS,
  &FileCompressionType::ORC,
  &FileCompressionType::PARQUET,
};

const FileCompressionType FileCompressionType::GZIP =
  FileCompressionType(GZIP_EXTENSION, GZIP_NAME, &gzip_magics, 2, true);

const FileCompressionType FileCompressionType::DEFLATE =
  FileCompressionType(DEFLATE_EXTENSION, DEFLATE_NAME, &deflate_magics, 2, true);

const FileCompressionType FileCompressionType::RAW_DEFLATE=
  FileCompressionType(RAW_DEFLATE_EXTENSION, RAW_DEFLATE_NAME, true);

const FileCompressionType FileCompressionType::BZIP2 =
  FileCompressionType(BZIP2_EXTENSION, BZIP2_NAME, &bzip2_magics, 2, true);

const FileCompressionType FileCompressionType::ZSTD =
  FileCompressionType(ZSTD_EXTENSION, ZSTD_NAME, &zstd_magics, 4, true);

const FileCompressionType FileCompressionType::BROTLI =
  FileCompressionType(BROTLI_EXTENSION, BROTLI_NAME, &brotli_magics, 4, true);

const FileCompressionType FileCompressionType::ORC =
  FileCompressionType(ORC_EXTENSION, ORC_NAME, &orc_magics, 3, true);

const FileCompressionType FileCompressionType::PARQUET =
  FileCompressionType(PARQUET_EXTENSION, PARQUET_NAME, &parquet_magics, 4, true);

const FileCompressionType FileCompressionType::LZIP =
  FileCompressionType(LZIP_EXTENSION, LZIP_NAME, false);

const FileCompressionType FileCompressionType::LZMA =
  FileCompressionType(LZMA_EXTENSION, LZMA_NAME, false);

const FileCompressionType FileCompressionType::LZO =
  FileCompressionType(LZO_EXTENSION, LZO_NAME, false);

const FileCompressionType FileCompressionType::XZ =
  FileCompressionType(XZ_EXTENSION, XZ_NAME, false);

const FileCompressionType FileCompressionType::COMPRESS =
  FileCompressionType(COMPRESS_EXTENSION, COMPRESS_NAME, false);

const FileCompressionType FileCompressionType::NONE =
  FileCompressionType(NONE_EXTENSION, NONE_NAME, true);

bool Snowflake::Client::FileCompressionType::matchMagicNumber(char *header) const
{
  if (m_magicNumbers)
  {
    for (size_t i = 0; i < m_magicNumbers->size(); i++)
    {
      if (strlen(header) >= m_magicBytes &&
        !memcmp(header, m_magicNumbers->at(i), m_magicBytes))
      {
        return true;
      }
    }
  }

  return false;
}

bool FileCompressionType::getIsSupported() const
{
  return m_isSupported;
}

const char * FileCompressionType::getFileExtension() const
{
  return m_fileExtension;
}

const FileCompressionType *FileCompressionType::
  guessCompressionType(std::string &fileFullPath)
{
  char header[MAX_MAGIC_BYTES + 1];
  memset(header, 0, sizeof(header));
  std::ifstream srcFile(fileFullPath.c_str(), std::fstream::in | std::ios::binary);
  // read first 4 bytes to determine compression type
  srcFile.read(header, MAX_MAGIC_BYTES);
  srcFile.close();

  for (size_t i=0; i< types.size(); i++)
  {
    if (types.at(i)->matchMagicNumber(header))
    {
      return types.at(i);
    }

    // For brotli type, add fallback logic that detection will depend on file
    // extension. This is because standard magic number is introduced in v3
    // format of brotli.
    // https://github.com/madler/brotli/blob/master/br-format-v3.txt
    if (types.at(i) == &FileCompressionType::BROTLI)
    {
      auto pos = fileFullPath.find_last_of('.');
      if( pos != std::string::npos) 
      {
        std::string file_ext = fileFullPath.substr(pos);
        if (!strcmp(file_ext.c_str(), types.at(i)->getFileExtension())) 
        {
          return types.at(i);
        }
      }
    }
  }

  // did not detect any type, process as if there is no compression type
  return &FileCompressionType::NONE;
}

const FileCompressionType *FileCompressionType::lookUpByName(const char *name)
{
  for (size_t i=0; i<types.size(); i++)
  {
    if (!sf_strncasecmp(name, (const char*)(types.at(i)->m_name), sizeof(types.at(i)->m_name)))
    {
      return types.at(i);
    }
  }

  return nullptr;
}

const char * FileCompressionType::getName() const
{
  return m_name;
}
