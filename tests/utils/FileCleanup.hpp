#pragma once
#include <string>

// RAII helper for automatic file cleanup
class FileCleanup
{
public:
  FileCleanup(const FileCleanup&) = delete;
  void operator=(const FileCleanup&) = delete;
  
  explicit FileCleanup(std::string filePath_)
    : filePath(std::move(filePath_)) {}
  
  ~FileCleanup()
  {
    if (!filePath.empty())
    {
      remove(filePath.c_str());
    }
  }
  
  const std::string& path() const { return filePath; }

private:
  std::string filePath;
};

