//
// Created by Jakub Szczerbinski on 15.10.24.
//

#include "FileLock.hpp"

#include <thread>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

#include "../logger/SFLogger.hpp"

namespace sf {
  using Snowflake::Client::SFLogger;

  constexpr int MAX_LOCK_RETRIES = 10;
  constexpr std::chrono::seconds lock_retry_sleep(1);
  constexpr const char *lock_suffix = ".lck";

  FileLock::FileLock(const std::string &path_)
      : path{path_ + lock_suffix},
        locked(false) {
    // Try to create file lock
    for (int retries = 0; !locked && retries < MAX_LOCK_RETRIES; retries++) {
      bool retry = try_lock();

      if (!retry) {
        break;
      }

      std::this_thread::sleep_for(lock_retry_sleep);
    }

    if (!locked) {
      CXX_LOG_INFO("Failed to acquire file lock(path=%s), resetting the lock", path.c_str());
      boost::system::error_code ec;
      bool removed = boost::filesystem::remove(path, ec);

      if (!removed || ec) {
        CXX_LOG_ERROR("Failed to reset file lock(path=%s), removing lock failed with: %d", path.c_str(), ec.value())
      } else {
        try_lock();
      }
    }
  }

  FileLock::~FileLock() {
    if (locked) {
      boost::system::error_code ec;
      bool removed = boost::filesystem::remove(path, ec);
      if (!removed || ec) {
        CXX_LOG_ERROR("Failed to release file lock(path=%s)", path.c_str());
      }
    }
  }

  bool FileLock::try_lock() {
    boost::system::error_code ec;
    bool created = boost::filesystem::create_directory(path, ec);

    if (ec) {
      CXX_LOG_ERROR("Failed to acquire file lock(path=%s) with error code: %d", path.c_str(), ec.value());
      return false;
    }

    if (!created) {
      CXX_LOG_INFO("Failed to acquire file lock(path=%s), lock already acquired");
      return true;
    }

    locked = true;
    return false;
  }
}
