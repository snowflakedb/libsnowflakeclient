/*
 * File:   FileLock.cpp *
 * Copyright (c) 2025 Snowflake Computing
 */

#include "FileLock.hpp"

#include <thread>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

#include "../logger/SFLogger.hpp"

namespace Snowflake {

namespace Client {

  using Snowflake::Client::SFLogger;

  constexpr int MAX_LOCK_RETRIES = 10;
  constexpr std::chrono::seconds lock_retry_sleep(1);
  constexpr const char *lock_suffix = ".lck";

  FileLock::FileLock(const std::string &path_)
      : path{path_ + lock_suffix},
        locked(false)
  {
    for (int retries = 0; !locked && retries < MAX_LOCK_RETRIES; retries++) {
      bool retry = try_lock();

      if (!retry) {
        break;
      }

      std::this_thread::sleep_for(lock_retry_sleep);
    }
    if (locked) {
      CXX_LOG_TRACE("Created file lock(path=%s)", path.c_str())
    }
  }

  FileLock::~FileLock() {
    if (locked) {
      boost::system::error_code ec;
      bool removed = boost::filesystem::remove(path, ec);
      if (!removed || ec) {
        CXX_LOG_ERROR("Failed to release file lock(path=%s)", path.c_str());
      }
      else {
        CXX_LOG_TRACE("Released file lock(path=%s)", path.c_str());
      }
    }
  }

  // Returns true if locking should be retried
  bool FileLock::try_lock() {
    boost::system::error_code ec;
    bool created = boost::filesystem::create_directory(path, ec);

    if (created && !ec)
    {
      locked = true;
      return false;
    }

    CXX_LOG_ERROR("Failed to acquire file lock(path=%s) with error code: %d", path.c_str(), ec.value());
    std::time_t creation_time_epoch_seconds = boost::filesystem::creation_time(path, ec);
    if (ec)
    {
      CXX_LOG_ERROR("Failed to get creation time for path=%s with error code: %d", path.c_str(), ec.value());
      return true;
    }

    std::time_t now_epoch_seconds = time(nullptr);
    if (creation_time_epoch_seconds + 60 < now_epoch_seconds)
    {
      CXX_LOG_INFO("Cleaning up stale lock.")
      boost::filesystem::remove(path, ec);
      if (ec)
      {
        CXX_LOG_ERROR("Failed to remove stale lock(path=%s) with error code: %d", path.c_str(), ec.value());
      }
    }

    return true;

  }
}

}
