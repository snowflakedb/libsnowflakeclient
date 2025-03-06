/*
 * Copyright (c) 2025 Snowflake Computing, Inc. All rights reserved.
 */
#include <boost/optional.hpp>

class EnvOverride
{
public:
  EnvOverride(const EnvOverride&) = delete;
  void operator=(const EnvOverride&) = delete;
  EnvOverride(std::string envVar_, const boost::optional<std::string>& newValue_)
    : envVar{std::move(envVar_)}
    , oldValue{}
  {
    char buf[BUF_SIZE];
    char* oldValueCstr = sf_getenv_s(envVar.c_str(), buf, BUF_SIZE);
    if (oldValueCstr != nullptr)
    {
      oldValue = oldValueCstr;
    }
    if (newValue_)
    {
      sf_setenv(envVar.c_str(), newValue_.get().c_str());
    }
    else
    {
      sf_unsetenv(envVar.c_str());
    }
  }

  EnvOverride(std::string envVar_, const std::string& newValue_)
    : EnvOverride(std::move(envVar_), boost::make_optional(newValue_)) {}

  ~EnvOverride()
  {
    if (oldValue)
    {
      sf_setenv(envVar.c_str(), oldValue->c_str());
    }
    else
    {
      sf_unsetenv(envVar.c_str());
    }
  }

private:
  constexpr static size_t BUF_SIZE = 1024;
  std::string envVar;
  boost::optional<std::string> oldValue;
};
