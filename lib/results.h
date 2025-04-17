#ifndef SNOWFLAKE_RESULTS_H
#define SNOWFLAKE_RESULTS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <snowflake/client.h>
#include "snowflake/platform.h"
#include "cJSON.h"

    SF_DB_TYPE string_to_snowflake_type(const char *string);
    SF_C_TYPE snowflake_to_c_type(SF_DB_TYPE type, int64 precision, int64 scale);
    SF_DB_TYPE c_type_to_snowflake(SF_C_TYPE c_type, SF_DB_TYPE tsmode);
    char *value_to_string(void *value, size_t len, SF_C_TYPE c_type);
    SF_COLUMN_DESC *set_description(SF_STMT *sfstmt, const cJSON *rowtype);
    SF_STATS *set_stats(cJSON *stats);

#ifdef __cplusplus
}
#endif

#endif // SNOWFLAKE_RESULTS_H
