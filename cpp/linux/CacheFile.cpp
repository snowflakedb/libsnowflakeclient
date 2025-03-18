#include "CacheFile.hpp"

#include <sys/stat.h>
#include <fcntl.h>

#include <string>

#include <picojson.h>
#include <boost/filesystem.hpp>

#include "../logger/SFLogger.hpp"
#include "../util/Sha256.hpp"

namespace {
  class FileDescriptor {
  public:
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor operator=(const FileDescriptor&) = delete;
    explicit FileDescriptor(int fd) : m_fd(fd) {}

    ~FileDescriptor() {
      close(m_fd);
    }
    operator int() const { return m_fd; }
    int get() const { return m_fd; }
  private:
    int m_fd;
  };

  using namespace Snowflake::Client;
  const char* CREDENTIAL_FILE_NAME = "credential_cache_v1.json";

  bool mkdirIfNotExists(const std::string& dir, mode_t mode)
  {
    int result = mkdir(dir.c_str(), mode);
    if (result == 0)
    {
      CXX_LOG_DEBUG("Created %s directory.", dir.c_str());
      return true;
    }

    if (errno == EEXIST)
    {
      CXX_LOG_TRACE("Directory %s already exists.", dir.c_str());
      return true;
    }

    CXX_LOG_ERROR("Failed to create %s directory. Error: %d", dir.c_str(), errno);
    return false;

  }

  boost::optional<std::string> getEnv(const std::string& envVar)
  {
    char *root = getenv(envVar.c_str());

    if (root == nullptr)
    {
      return {};
    }

    return std::string(root);
  }

  void ensureObject(picojson::value &val)
  {
    if (!val.is<picojson::object>())
    {
      val = picojson::value(picojson::object());
    }
  }

  picojson::object& getTokens(picojson::value& cache)
  {
    ensureObject(cache);
    auto &obj = cache.get<picojson::object>();
    auto pair = obj.emplace("tokens", picojson::value(picojson::object()));
    auto& tokens = pair.first->second;
    ensureObject(tokens);
    return tokens.get<picojson::object>();
  }

  bool ensurePermissions(const FileDescriptor& fd, mode_t mode)
  {
    struct stat s = {};
    int err = fstat(fd, &s);
    if (err == -1)
    {
      CXX_LOG_ERROR("Cannot ensure permissions. fstat(%d, %o) failed with errno=%d", fd.get(), mode, errno);
      return false;
    }

    if (s.st_uid != geteuid())
    {
      CXX_LOG_ERROR("Cannot ensure permissions. Cache file is not owned by effective user.");
      return false;
    }

    if ((s.st_mode & 0777) != mode)
    {
      CXX_LOG_ERROR("Cannot ensure permissions. Cache file permissions are %o != %o", s.st_mode & 0777, mode);
      return false;
    }

    return true;
  }
}

namespace Snowflake {

namespace Client {
  boost::optional<std::string> getCacheDir(const std::string& envVar, const std::vector<std::string>& subPathSegments)
  {
    auto envVarValueOpt = getEnv(envVar);
    if (!envVarValueOpt)
    {
      return {};
    }

    const std::string& envVarValue = envVarValueOpt.get();

    struct stat s = {};
    int err = stat(envVarValue.c_str(), &s);

    if (err != 0)
    {
      CXX_LOG_INFO("Failed to stat %s=%s, errno=%d. Skipping it in cache file location lookup.", envVar.c_str(), envVarValue.c_str(), errno);
      return {};
    }

    if (!S_ISDIR(s.st_mode))
    {
      CXX_LOG_INFO("%s=%s is not a directory. Skipping it in cache file location lookup.", envVar.c_str(), envVarValue.c_str());
      return {};
    }

    auto cacheDir = envVarValue;
    for (const auto &segment: subPathSegments) {
      if (!mkdirIfNotExists(cacheDir, 0755)) {
        CXX_LOG_INFO("Could not create cache dir=%s. Skipping it in cache file location lookup.", cacheDir.c_str());
        return {};
      }
      cacheDir.append(PATH_SEP + segment);
    }

    if (!mkdirIfNotExists(cacheDir, 0700)) {
      CXX_LOG_INFO("Could not create cache dir=%s. Skipping it in cache file location lookup.", cacheDir.c_str());
      return {};
    }

    err = stat(cacheDir.c_str(), &s);

    if (err != 0)
    {
      CXX_LOG_INFO("Failed to stat %s, errno=%d. Skipping it in cache file location lookup.", envVar.c_str(), errno);
      return {};
    }

    if (!S_ISDIR(s.st_mode))
    {
      CXX_LOG_INFO("%s is not a directory. Skipping it in cache file location lookup.", cacheDir.c_str());
      return {};
    }

    if (s.st_uid != geteuid())
    {
      CXX_LOG_INFO("%s is not owned by effective user. Skipping it in cache file location lookup.", cacheDir.c_str());
      return {};
    }

    if ((s.st_mode & 0777) != 0700)
    {
      CXX_LOG_INFO("%s has incorrect permissions. Skipping it in cache file location lookup.", cacheDir.c_str());
      return {};
    }

    return cacheDir;
  }

  boost::optional<std::string> getCredentialFilePath()
  {
    std::vector<std::function<boost::optional<std::string>()>> lookupFunctions =
    {
        []() { return getCacheDir("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", {}); },
        [](){ return getCacheDir("XDG_CACHE_HOME", {"snowflake"}); },
        [](){ return getCacheDir("HOME", {".cache", "snowflake"}); },
    };

    for (const auto& lf: lookupFunctions) {
      boost::optional<std::string> directory = lf();
      if (directory)
      {
        auto path = directory.get() + PATH_SEP + CREDENTIAL_FILE_NAME;
        CXX_LOG_TRACE("Successfully found credential file path=%s", path.c_str());
        return path;
      }
    }

    return {};
  };

  std::string readFile(const std::string &path, picojson::value &result) {
    FileDescriptor fd{open(path.c_str(), O_RDONLY)};
    if (fd == -1) {
      if (errno == ENOENT) {
        result = picojson::value(picojson::object());
        return {};
      }
      else {
        return "Failed to open " + path + " errno=" + std::to_string(errno);
      }
    }

    if (!ensurePermissions(fd, 0600))
    {
      return "Failed to ensure permissions for file(path=" + path + ")";
    }

    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize == -1) {
      return "Failed to seek end of file(path=" + path + ")";
    }
    if (lseek(fd, 0, SEEK_SET) == -1) {
      return "Failed to seek start of file(path=" + path + ")";
    }

    std::string fileContents;
    fileContents.resize(fileSize);
    ssize_t bytesRead = read(fd, fileContents.data(), fileSize);
    if (bytesRead == -1) {
      return "Failed to read file(path=" + path + ", errno=" + std::to_string(errno) + ")";
    }
    fileContents.resize(bytesRead);

    std::string error = picojson::parse(result, fileContents);
    if (!error.empty())
    {
      return "Failed to parse the file: " + error;
    }
    return {};
  }

  std::string writeFile(const std::string &path, const picojson::value &result) {
    FileDescriptor fd{open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600)};
    if (fd == -1)
    {
      return "Failed to open the file(path=" + path + ")";
    }

    if (!ensurePermissions(fd, 0600))
    {
      return "Cannot ensure correct permissions on a file";
    }

    std::string buf = result.serialize();
    int bytesWritten = write(fd, buf.data(), buf.size());
    if (bytesWritten == -1) {
      return "Failed to write to a file(path=" + path + ", errno=" + std::to_string(errno) + ")";
    }
    return {};
  }

  void cacheFileUpdate(picojson::value &cache, const std::string &key, const std::string &credential)
  {
    picojson::object& tokens = getTokens(cache);
    tokens[key] = picojson::value(credential);
  }

  void cacheFileRemove(picojson::value &cache, const std::string &key)
  {
    picojson::object& tokens = getTokens(cache);
    tokens.erase(key);
  }

  boost::optional<std::string> cacheFileGet(picojson::value &cache, const std::string &key) {
    picojson::object& tokens = getTokens(cache);
    auto it = tokens.find(key);

    if (it == tokens.end())
    {
      return {};
    }

    if (!it->second.is<std::string>())
    {
      return {};
    }

    return it->second.get<std::string>();
  }

}

}
