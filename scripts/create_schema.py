#!/usr/bin/env python
#
# Create test schema
#
import os
import sys
import snowflake.connector

import sf_test_utils

test_schema = sf_test_utils.get_test_schema()
params = sf_test_utils.init_connection_params()

con = snowflake.connector.connect(**params)
con.cursor().execute("create or replace schema {0} clone C_API_TEST_DATA_SCHEMA".format(test_schema))

sys.exit(0)