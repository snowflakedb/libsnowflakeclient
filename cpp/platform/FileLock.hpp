//
// Created by Jakub Szczerbinski on 15.10.24.
//

#ifndef SNOWFLAKECLIENT_FILELOCK_HPP
#define SNOWFLAKECLIENT_FILELOCK_HPP

#include <string>

namespace sf {
  class FileLock {
  public:
    explicit FileLock(const std::string& path);

    // Disable copying
    FileLock(const FileLock &) = delete;
    FileLock &operator=(const FileLock &) = delete;

    inline bool isLocked() const {
      return locked;
    }

    inline const std::string &getPath() const {
      return path;
    }

    ~FileLock();

  private:
    bool try_lock();

    std::string path;
    bool locked;
  };
}


#endif //SNOWFLAKECLIENT_FILELOCK_HPP
