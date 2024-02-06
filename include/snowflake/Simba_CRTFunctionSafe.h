/*
 * Copyright (c) 2019-2024 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef _SIMBA_CRTFUNCTIONSAFE_H_
#define _SIMBA_CRTFUNCTIONSAFE_H_

#include "SF_CRTFunctionSafe.h"

SF_MACRO_DEPRECATED_WARNING("Simba_CRTFunctionSafe.h and all functions declared there are deprecated. Please include SF_CRTFunctionSafe.h and use functions there instead.")

#define sb_memcpy SF_MACRO_DEPRECATED_WARNING("sb_memcpy is deprecated, please use sf_memcpy instead.") sf_memcpy
#define sb_copy SF_MACRO_DEPRECATED_WARNING("sb_copy is deprecated, please use sf_copy instead.") sf_copy
#define sb_cat SF_MACRO_DEPRECATED_WARNING("sb_cat is deprecated, please use sf_cat instead.") sf_cat
#define sb_strcpy SF_MACRO_DEPRECATED_WARNING("sb_strcpy is deprecated, please use sf_strcpy instead.") sf_strcpy
#define sb_strncpy SF_MACRO_DEPRECATED_WARNING("sb_strncpy is deprecated, please use sf_strncpy instead.") sf_strncpy
#define sb_strcat SF_MACRO_DEPRECATED_WARNING("sb_strcat is deprecated, please use sf_strcat instead.") sf_strcat
#define sb_strncat SF_MACRO_DEPRECATED_WARNING("sb_strncat is deprecated, please use sf_strncat instead.") sf_strncat
#define sb_vsnprintf SF_MACRO_DEPRECATED_WARNING("sb_vsnprintf is deprecated, please use sf_vsnprintf instead.") sf_vsnprintf
#define sb_sprintf SF_MACRO_DEPRECATED_WARNING("sb_sprintf is deprecated, please use sf_sprintf instead.") sf_sprintf
#define sb_snprintf SF_MACRO_DEPRECATED_WARNING("sb_snprintf is deprecated, please use sf_snprintf instead.") sf_snprintf
#define sb_sscanf SF_MACRO_DEPRECATED_WARNING("sb_sscanf is deprecated, please use sf_sscanf instead.") sf_sscanf
#define sb_vfprintf SF_MACRO_DEPRECATED_WARNING("sb_vfprintf is deprecated, please use sf_vfprintf instead.") sf_vfprintf
#define sb_fprintf SF_MACRO_DEPRECATED_WARNING("sb_fprintf is deprecated, please use sf_fprintf instead.") sf_fprintf
#define sb_printf SF_MACRO_DEPRECATED_WARNING("sb_printf is deprecated, please use sf_printf instead.") sf_printf
#define sb_fopen SF_MACRO_DEPRECATED_WARNING("sb_fopen is deprecated, please use sf_fopen instead.") sf_fopen

#endif
