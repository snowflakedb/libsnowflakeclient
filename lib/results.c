/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <string.h>
#include "results.h"
#include "connection.h"
#include "memory.h"
#include <snowflake/logger.h>

static size_t _bin2hex(
  char *dst, const char *src, size_t dst_max_len, size_t src_len) {
    size_t i = 0;
    size_t dlen = 0;
    if (dst_max_len < src_len * 2) {
        return 0;
    }
    for (i=0, dlen=0; i < src_len && dlen < dst_max_len; ++i, dlen+=2) {
        unsigned char hb = (unsigned char)src[i] >> 4;
        unsigned char lb = (unsigned char)src[i] & (unsigned char)0xf;
        if (hb > 9) {
            dst[i*2] = (unsigned char)'A' + (hb - (unsigned char) 10);
        } else {
            dst[i*2] = (unsigned char)'0' + hb;
        }
        if (lb > 9) {
            dst[i*2+1] = (unsigned char)'A' + (lb - (unsigned char) 10);
        } else {
            dst[i*2+1] = (unsigned char)'0' + lb;
        }
    }
    return dlen;
}

SF_DB_TYPE string_to_snowflake_type(const char *string) {
    if (strcmp(string, "fixed") == 0) {
        return SF_DB_TYPE_FIXED;
    } else if (strcmp(string, "real") == 0) {
        return SF_DB_TYPE_REAL;
    } else if (strcmp(string, "text") == 0) {
        return SF_DB_TYPE_TEXT;
    } else if (strcmp(string, "date") == 0) {
        return SF_DB_TYPE_DATE;
    } else if (strcmp(string, "timestamp_ltz") == 0) {
        return SF_DB_TYPE_TIMESTAMP_LTZ;
    } else if (strcmp(string, "timestamp_ntz") == 0) {
        return SF_DB_TYPE_TIMESTAMP_NTZ;
    } else if (strcmp(string, "timestamp_tz") == 0) {
        return SF_DB_TYPE_TIMESTAMP_TZ;
    } else if (strcmp(string, "variant") == 0) {
        return SF_DB_TYPE_VARIANT;
    } else if (strcmp(string, "object") == 0) {
        return SF_DB_TYPE_OBJECT;
    } else if (strcmp(string, "array") == 0) {
        return SF_DB_TYPE_ARRAY;
    } else if (strcmp(string, "binary") == 0) {
        return SF_DB_TYPE_BINARY;
    } else if (strcmp(string, "time") == 0) {
        return SF_DB_TYPE_TIME;
    } else if (strcmp(string, "boolean") == 0) {
        return SF_DB_TYPE_BOOLEAN;
    } else if (strcmp(string, "any") == 0) {
        return SF_DB_TYPE_ANY;
    } else {
        // Everybody loves a string, so lets return it by default
        return SF_DB_TYPE_TEXT;
    }
}

const char *STDCALL snowflake_type_to_string(SF_DB_TYPE type) {
    switch (type) {
        case SF_DB_TYPE_FIXED:
            return "FIXED";
        case SF_DB_TYPE_REAL:
            return "REAL";
        case SF_DB_TYPE_TEXT:
            return "TEXT";
        case SF_DB_TYPE_DATE:
            return "DATE";
        case SF_DB_TYPE_TIMESTAMP_LTZ:
            return "TIMESTAMP_LTZ";
        case SF_DB_TYPE_TIMESTAMP_NTZ:
            return "TIMESTAMP_NTZ";
        case SF_DB_TYPE_TIMESTAMP_TZ:
            return "TIMESTAMP_TZ";
        case SF_DB_TYPE_VARIANT:
            return "VARIANT";
        case SF_DB_TYPE_OBJECT:
            return "OBJECT";
        case SF_DB_TYPE_ARRAY:
            return "ARRAY";
        case SF_DB_TYPE_BINARY:
            return "BINARY";
        case SF_DB_TYPE_TIME:
            return "TIME";
        case SF_DB_TYPE_BOOLEAN:
            return "BOOLEAN";
        case SF_DB_TYPE_ANY:
            return "ANY";
        default:
            return "TEXT";
    }
}

const char * STDCALL snowflake_c_type_to_string(SF_C_TYPE type) {
    switch (type) {
        case SF_C_TYPE_STRING:
            return "SF_C_TYPE_STRING";
        case SF_C_TYPE_UINT8:
            return "SF_C_TYPE_UINT8";
        case SF_C_TYPE_INT8:
            return "SF_C_TYPE_INT8";
        case SF_C_TYPE_UINT64:
            return "SF_C_TYPE_UINT64";
        case SF_C_TYPE_INT64:
            return "SF_C_TYPE_INT64";
        case SF_C_TYPE_FLOAT64:
            return "SF_C_TYPE_FLOAT64";
        case SF_C_TYPE_BOOLEAN:
            return "SF_C_TYPE_BOOLEAN";
        case SF_C_TYPE_TIMESTAMP:
            return "SF_C_TYPE_TIMESTAMP";
        case SF_C_TYPE_BINARY:
            return "SF_C_TYPE_BINARY";
        case SF_C_TYPE_NULL:
            return "SF_C_TYPE_NULL";
        default:
            return "unknown";
    }
}


SF_C_TYPE snowflake_to_c_type(SF_DB_TYPE type, int64 precision, int64 scale) {
    if (type == SF_DB_TYPE_FIXED) {
        if (scale > 0) {
            return SF_C_TYPE_FLOAT64;
        } else {
            return SF_C_TYPE_INT64;
        }
    } else if (type == SF_DB_TYPE_REAL) {
        return SF_C_TYPE_FLOAT64;
    } else if (type == SF_DB_TYPE_TIMESTAMP_LTZ ||
            type == SF_DB_TYPE_TIMESTAMP_NTZ ||
            type == SF_DB_TYPE_TIMESTAMP_TZ) {
        return SF_C_TYPE_TIMESTAMP;
    } else if (type == SF_DB_TYPE_BOOLEAN) {
        return SF_C_TYPE_BOOLEAN;
    } else if (type == SF_DB_TYPE_TEXT ||
            type == SF_DB_TYPE_VARIANT ||
            type == SF_DB_TYPE_OBJECT ||
            type == SF_DB_TYPE_ARRAY ||
            type == SF_DB_TYPE_ANY) {
        return SF_C_TYPE_STRING;
    } else if (type == SF_DB_TYPE_BINARY) {
        return SF_C_TYPE_BINARY;
    } else {
        // by default return string, since we can do everything with a string
        return SF_C_TYPE_STRING;
    }
}

SF_DB_TYPE c_type_to_snowflake(SF_C_TYPE c_type, SF_DB_TYPE tsmode) {
    switch (c_type) {
        case SF_C_TYPE_INT8:
            return SF_DB_TYPE_FIXED;
        case SF_C_TYPE_UINT8:
            return SF_DB_TYPE_FIXED;
        case SF_C_TYPE_INT64:
            return SF_DB_TYPE_FIXED;
        case SF_C_TYPE_UINT64:
            return SF_DB_TYPE_FIXED;
        case SF_C_TYPE_FLOAT64:
            return SF_DB_TYPE_REAL;
        case SF_C_TYPE_STRING:
            return SF_DB_TYPE_TEXT;
        case SF_C_TYPE_TIMESTAMP:
            return tsmode;
        case SF_C_TYPE_BOOLEAN:
            return SF_DB_TYPE_BOOLEAN;
        case SF_C_TYPE_BINARY:
            return SF_DB_TYPE_BINARY;
        case SF_C_TYPE_NULL:
            return SF_DB_TYPE_ANY;
        default:
            return SF_DB_TYPE_TEXT;
    }
}

char *value_to_string(void *value, size_t len, SF_C_TYPE c_type) {
    size_t size;
    char *ret;
    if (value == NULL) {
        return NULL;
    }

    // The buffer size for fixed length data types.
    // Not necessarily to be exact length of the actual data, to avoid memory fragmentation.
    size = 64;

    // TODO turn cases into macro and check to see if ret if null
    switch (c_type) {
        case SF_C_TYPE_INT8:
            ret = (char *) SF_CALLOC(1, size);
            sb_sprintf(ret, size, "%d", *(int8 *) value);
            return ret;
        case SF_C_TYPE_UINT8:
            ret = (char *) SF_CALLOC(1, size);
            sb_sprintf(ret, size, "%u", *(uint8 *) value);
            return ret;
        case SF_C_TYPE_INT64:
            ret = (char *) SF_CALLOC(1, size);
            sb_sprintf(ret, size, "%lld", *(int64 *) value);
            return ret;
        case SF_C_TYPE_UINT64:
            ret = (char *) SF_CALLOC(1, size);
            sb_sprintf(ret, size, "%llu", *(uint64 *) value);
            return ret;
        case SF_C_TYPE_FLOAT64:
            ret = (char *) SF_CALLOC(1, size);
            sb_sprintf(ret, size, "%f", *(float64 *) value);
            return ret;
        case SF_C_TYPE_BOOLEAN:
            ret = (char*) SF_CALLOC(1, size + 1);
            sb_strncpy(ret, size + 1, *(sf_bool*)value != (sf_bool)0 ? SF_BOOLEAN_INTERNAL_TRUE_STR : SF_BOOLEAN_INTERNAL_FALSE_STR, size + 1);
            return ret;
        case SF_C_TYPE_BINARY:
            size = (size_t)len * 2 + 1;
            ret = (char*) SF_CALLOC(1, size);
            _bin2hex(ret, (const char *) value, size - 1, (size_t) len);
            ret[size-1] = '\0';
            return ret;
        case SF_C_TYPE_STRING:
            size = (size_t)len + 1;
            ret = (char *) SF_CALLOC(1, size);
            sb_strncpy(ret, size, (const char *) value, size);
            return ret;
        case SF_C_TYPE_TIMESTAMP:
            // TODO Add timestamp case
            return "";
        case SF_C_TYPE_NULL:
            return NULL;
        default:
            // TODO better default case
            // Return empty string in default case
            ret = (char *) SF_CALLOC(1, 1);
            ret[0] = '\0';
            return ret;
    }
}

SF_COLUMN_DESC * set_description(const cJSON *rowtype) {
    int i;
    cJSON *blob;
    cJSON *column;
    SF_COLUMN_DESC *desc = NULL;
    size_t array_size = (size_t) snowflake_cJSON_GetArraySize(rowtype);
    if (rowtype == NULL || array_size == 0) {
        return desc;
    }
    desc = (SF_COLUMN_DESC *) SF_CALLOC(array_size, sizeof(SF_COLUMN_DESC));
    for (i = 0; i < (int)array_size; i++) {
        column = snowflake_cJSON_GetArrayItem(rowtype, i);
        // Index starts at 1
        desc[i].idx = (size_t) i + 1;
        if(json_copy_string(&desc[i].name, column, "name")) {
            desc[i].name = NULL;
        }
        if (json_copy_int(&desc[i].byte_size, column, "byteLength")) {
            desc[i].byte_size = 0;
        }
        if (json_copy_int(&desc[i].internal_size, column, "length")) {
            desc[i].internal_size = 0;
        }
        if (json_copy_int(&desc[i].precision, column, "precision")) {
            desc[i].precision = 0;
        }
        if (json_copy_int(&desc[i].scale, column, "scale")) {
            desc[i].scale = 0;
        }
        if (json_copy_bool(&desc[i].null_ok, column, "nullable")) {
            desc[i].null_ok = SF_BOOLEAN_FALSE;
        }
        // Get type
        blob = snowflake_cJSON_GetObjectItem(column, "type");
        if (snowflake_cJSON_IsString(blob)) {
            desc[i].type = string_to_snowflake_type(blob->valuestring);
        } else {
            // TODO Replace with default type
            desc[i].type = SF_DB_TYPE_FIXED;
        }
        desc[i].c_type = snowflake_to_c_type(desc[i].type, desc[i].precision, desc[i].scale);
        log_debug("Found type and ctype; %i: %i", desc[i].type, desc[i].c_type);

    }

    return desc;
}

SF_STATS * set_stats(cJSON *stats) {
    SF_STATS *metadata = NULL;
    metadata = (SF_STATS *) SF_MALLOC(sizeof(SF_STATS));
    if(json_copy_int(&metadata->num_rows_inserted, stats, "numRowsInserted")) {
        metadata->num_rows_inserted = 0;
    }
    if(json_copy_int(&metadata->num_rows_updated, stats, "numRowsUpdated")) {
        metadata->num_rows_updated = 0;
    }
    if(json_copy_int(&metadata->num_rows_deleted, stats, "numRowsDeleted")) {
        metadata->num_rows_deleted = 0;
    }
    if(json_copy_int(&metadata->num_duplicate_rows_updated, stats, "numDmlDuplicates")) {
        metadata->num_duplicate_rows_updated = 0;
    }
    return metadata;
}
