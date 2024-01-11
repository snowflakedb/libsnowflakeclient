/*
 * Copyright (c) 2018-2024 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_UTIL_H
#define SNOWFLAKE_UTIL_H

#include "snowflake/platform.h"

// a smart pointer that free memory allocated by stdlib (malloc/calloc/realloc)
template <typename T=char>
class basic_cbuf_t {
public:
    basic_cbuf_t(T* p) : p_(p) {}
    ~basic_cbuf_t() { if (p_) free(p_); }
    T* value() { return p_; }
    operator T* () { return p_; }
    operator const T* () { return p_; }
    operator bool () { return p_ != NULL; }
private:
    T* p_;
};
typedef basic_cbuf_t<char> cbuf_t;

// try to get alternative environment variables
template <typename T=char>
T* sf_getenv_or(const char* name) {
    return sf_getenv(name);
}
template <typename T=char, typename... Args>
T* sf_getenv_or(const char* prime, Args... fallback)
{
    T* value = sf_getenv_or<T>(prime);
    return value ? value : sf_getenv_or<T>(fallback...);
}

#endif
