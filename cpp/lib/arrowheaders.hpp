#pragma once
#ifndef __SF_ARROW_HEADERS_HPP
#define __SF_ARROW_HEADERS_HPP

#include <memory>
#include <string>
#include <cstdint>
#include <vector>

/*
  Apache arrow is not implemented for WIN32
  The symbol _WIN32 is defined by the compiler to indicate that this is a (32bit) Windows compilation. 
  Unfortunately, for historical reasons, it is also defined for 64-bit compilation.
  The symbol _WIN64 is defined by the compiler to indicate that this is a 64-bit Windows compilation.
  Thus:
  To identify unambiguously whether the compilation is 32-bit Windows, one tests both _WIN32 and _WIN64 as in:
*/
#if defined(_WIN64)
#define SF_WIN64
#elif defined(_WIN32)
#define SF_WIN32
#endif

#ifndef SF_WIN32

#undef BOOL              //Arrow redefines BOOL

#ifdef _WIN64
#undef max
#undef min

#define ARROW_STATIC 

#endif

#include <arrow/api.h>
#include <arrow/io/api.h>
#include "arrow/ipc/reader.h"
#include "arrow/ipc/writer.h"
#include "arrow/ipc/options.h"
#include "arrow/record_batch.h"
#include "arrow/status.h"
#include "arrow/util/io_util.h"
#include "arrow/util/base64.h"
#include "arrow/util/basic_decimal.h"
#include "arrow/util/decimal.h"
#include "arrow/table.h"

#endif

#endif
