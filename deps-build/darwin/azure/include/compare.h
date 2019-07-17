#pragma once

#ifdef __linux__
#include <string.h>
#else
#include <string>
#endif

namespace azure { namespace storage_lite {
    // Case insensitive comparator for std::map comparator
    struct case_insensitive_compare 
    {
        bool operator()(const std::string &lhs, const std::string &rhs) const 
        {
#ifdef _WIN32
            return _stricmp(lhs.c_str(), rhs.c_str()) < 0;
#else
            return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
#endif

        }
    };
}} // azure::storage_lite
