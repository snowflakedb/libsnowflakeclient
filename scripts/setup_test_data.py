#!/usr/bin/env python
#
# Setup test data
#
import os
import sys
import snowflake.connector

import sf_test_utils

csv_file_path = os.path.abspath(os.path.join(os.path.abspath(__file__), "../../tests/data/public_domain_books.csv"))

# Start of SQL commands
create_test_data_schema = """
create schema if not exists C_API_TEST_DATA_SCHEMA;
"""

use_test_data_schema = """
use schema C_API_TEST_DATA_SCHEMA;
"""

create_test_table = """
create table if not exists public_domain_books (
    id int, 
    title VARCHAR(1048576), 
    text VARCHAR(1048576), 
    text_part int
)
"""

check_table_data = """
select count(*) from public_domain_books;
"""

delete_table_data = """
delete from public_domain_books;
"""

put_csv_file = """
put file://{0} @%public_domain_books;
""".format(csv_file_path)

copy_into_table = """
copy into public_domain_books file_format=(
    type=csv 
    skip_header=0 
    compression='auto' 
    field_optionally_enclosed_by='"' 
    field_delimiter=',' 
    record_delimiter='\\n');
"""

remove_csv_file = """
remove @%public_domain_books;
"""

params = sf_test_utils.init_connection_params()

# Initialize connection
con = snowflake.connector.connect(**params)
print('Connected to Snowflake')

# Create schema and table for test data
con.cursor().execute(create_test_data_schema)
print('Test Schema Created (or already exists)')
con.cursor().execute(use_test_data_schema)
con.cursor().execute(create_test_table)
print('Test Table Created (or already exists)')

# Check to see if data already exists in table
length = con.cursor().execute(check_table_data).fetchone()[0]

# If data does not already exist or is bad, then repopulate table with test data
if length != 14:
    print('Test data is being uploaded')
    con.cursor().execute(delete_table_data)
    con.cursor().execute(put_csv_file)
    print('CSV file has been uploaded to table stage')
    con.cursor().execute(copy_into_table)
    print('Test data has been loaded into table')
    con.cursor().execute(remove_csv_file)
else:
    print('Test data already exists in account, no data population needed')

sys.exit(0)
