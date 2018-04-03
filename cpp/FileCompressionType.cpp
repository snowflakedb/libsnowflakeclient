/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <fstream>
#include <ios>
#include <cstring>
#include "FileCompressionType.hpp"

using Snowflake::Client::FileCompressionType;

#define GZIP_MAGIC "\x1f\x8b"

std::vector<const char *> gzip_magics = {GZIP_MAGIC};

Snowflake::Client::FileCompressionType::FileCompressionType(
  const char * fileExtension,
  bool isSupported){
  Snowflake::Client::FileCompressionType(fileExtension, {}, 0, isSupported);
}

Snowflake::Client::FileCompressionType::FileCompressionType(
  const char * fileExtension,
  std::vector<const char *> *magicNumbers,
  short magicBytes,
  bool isSupported){
  m_fileExtension = fileExtension;
  m_isSupported = isSupported;
  m_magicNumbers = magicNumbers;
  m_magicBytes = magicBytes;
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
  FileCompressionType(".gz", &gzip_magics, 2, true);

const FileCompressionType FileCompressionType::DEFLATE =
  FileCompressionType(".deflate", true);

const Snowflake::Client::FileCompressionType
  Snowflake::Client::FileCompressionType::RAW_DEFLATE=
  Snowflake::Client::FileCompressionType(".raw_deflate", true);

const Snowflake::Client::FileCompressionType
  Snowflake::Client::FileCompressionType::BZIP2 =
  Snowflake::Client::FileCompressionType(".bz2", true);

const Snowflake::Client::FileCompressionType
  Snowflake::Client::FileCompressionType::ZSTD =
  Snowflake::Client::FileCompressionType(".zst", true);

const Snowflake::Client::FileCompressionType
  Snowflake::Client::FileCompressionType::BROTLI =
  Snowflake::Client::FileCompressionType(".br", true);

const Snowflake::Client::FileCompressionType
  Snowflake::Client::FileCompressionType::ORC =
  Snowflake::Client::FileCompressionType(".orc", true);

const Snowflake::Client::FileCompressionType
  Snowflake::Client::FileCompressionType::PARQUET =
  Snowflake::Client::FileCompressionType(".parquet", true);

const Snowflake::Client::FileCompressionType
  Snowflake::Client::FileCompressionType::LZIP =
  Snowflake::Client::FileCompressionType(".lz", false);

const Snowflake::Client::FileCompressionType
  Snowflake::Client::FileCompressionType::LZMA =
  Snowflake::Client::FileCompressionType(".lzma", false);

const Snowflake::Client::FileCompressionType
  Snowflake::Client::FileCompressionType::LZO =
  Snowflake::Client::FileCompressionType(".lzo", false);

const Snowflake::Client::FileCompressionType
  Snowflake::Client::FileCompressionType::XZ =
  Snowflake::Client::FileCompressionType(".xz", false);

const Snowflake::Client::FileCompressionType
  Snowflake::Client::FileCompressionType::COMPRESS =
  Snowflake::Client::FileCompressionType(".Z", false);

const Snowflake::Client::FileCompressionType
  Snowflake::Client::FileCompressionType::NONE =
  Snowflake::Client::FileCompressionType("", true);

bool Snowflake::Client::FileCompressionType::matchMagicNumber(char *header) const
{
  if (m_magicNumbers)
  {
    for (size_t i = 0; i < m_magicNumbers->size(); i++)
    {
      if (!memcmp(header, m_magicNumbers->at(i), m_magicBytes))
      {
        return true;
      }
    }
    return false;
  }
  else
  {
    return false;
  }
}

bool Snowflake::Client::FileCompressionType::matchSubMimeType(
  const char *subMime) const
{
  for (size_t i=0; i<m_subMimeTypes.size(); i++)
  {
    if (strcmp(subMime, m_subMimeTypes.at(i)) == 0)
    {
      return true;
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
  char header[4] = {0};
  std::ifstream srcFile(fileFullPath.c_str(), std::fstream::in | std::ios::binary);
  // read first 4 bytes to determine compression type
  srcFile.read(header, sizeof(header));
  srcFile.close();

  for (size_t i=0; i< types.size(); i++)
  {
    if (types.at(i)->matchMagicNumber(header))
    {
      return types.at(i);
    }
  }

  // did not detect any type, process as if there is no compression type
  return &FileCompressionType::NONE;
}

const FileCompressionType *FileCompressionType::
  lookUpBySubMime(const char *subMime)
{
  for (size_t i=0; i<types.size(); i++)
  {
    if (types.at(i)->matchSubMimeType(subMime))
    {
      return types.at(i);
    }
  }

  return nullptr;
}
