#include <cstdlib>
#include "TestSetup.hpp"

std::string TestSetup::getDataDir()
{
  return std::string("data") + PATH_SEP;
}
