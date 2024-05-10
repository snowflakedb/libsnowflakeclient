#!/bin/bash -e
#
# generate code coverage report on Linux
#

# enable code coverage and rebuild libsnowflakeclient
export CLIENT_CODE_COVERAGE=1
./scripts/build_libsnowflakeclient.sh

# Please edit here to specify the path of cacert.pem
export SNOWFLAKE_TEST_CA_BUNDLE_FILE=

# Please edit here to add AWS account information for test
export SNOWFLAKE_TEST_ROLE=
export SNOWFLAKE_TEST_USER=
export SNOWFLAKE_TEST_ACCOUNT=
export SNOWFLAKE_TEST_DATABASE=
export SNOWFLAKE_TEST_WAREHOUSE=
export SNOWFLAKE_TEST_SCHEMA=
export SNOWFLAKE_TEST_HOST=
export SNOWFLAKE_TEST_PASSWORD=
export CLOUD_PROVIDER=AWS

# run tests with AWS account
cd cmake-build-Release/tests
./test_adjust_fetch_data
./test_binary
cd test_bind_datastructures/unit_tests
./test_unit_rbtree
./test_unit_treemap
cd ../..
./test_bind_named_params
./test_bind_params
./test_bool
./test_change_current
./test_check_ctypes
./test_column_fetch
./test_connect
./test_connect_negative
./test_crud
./test_error_handlings
./test_get_describe_only_query_result
./test_get_query_result_response
./test_include_aws
./test_issue_76
./test_jwt
./test_large_result_set
./test_native_timestamp
./test_null
./test_number
./test_parallel_upload_download
./test_ping_pong
./test_select1
./test_simple_put
./test_stmt_with_bad_connect
./test_stmt_functions
./test_time
./test_timestamp_ltz
./test_timestamp_ntz
./test_timestamp_tz
./test_timezone
./test_transaction
./test_unit_base64
./test_unit_cipher_stream_buf
./test_unit_connect_parameters
./test_unit_cred_renew
./test_unit_file_metadata_init
./test_unit_file_type_detect
./test_unit_jwt
./test_unit_logger
./test_unit_oob
./test_unit_proxy
./test_unit_put_fast_fail
./test_unit_put_get_fips
./test_unit_put_get_gcs
./test_unit_put_retry
./test_unit_query_context_cache
./test_unit_retry_context
./test_unit_stream_splitter
./test_unit_thread_pool
./test_variant
./test_unit_set_get_attributes
./test_unit_snowflake_types_to_string
./test_unit_sfurl
cd unit_test_ocsp
./test_ocsp
cd ..

# Please edit here to add AZURE account information for test
export SNOWFLAKE_TEST_ROLE=
export SNOWFLAKE_TEST_USER=
export SNOWFLAKE_TEST_ACCOUNT=
export SNOWFLAKE_TEST_DATABASE=
export SNOWFLAKE_TEST_WAREHOUSE=
export SNOWFLAKE_TEST_SCHEMA=
export SNOWFLAKE_TEST_HOST=
export SNOWFLAKE_TEST_PASSWORD=
export CLOUD_PROVIDER=AZURE

# run put get test with AZURE account
./test_parallel_upload_download
./test_simple_put
./test_unit_azure_client

# generate code coverage report
cd ../..
mkdir -p code_coverage
lcov -c -d ./cmake-build-Release/CMakeFiles/snowflakeclient.dir/ --output-file ./code_coverage/client_coverage_all.info
# remove source code of third-parties
lcov --remove ./code_coverage/client_coverage_all.info -o ./code_coverage/client_coverage.info '/usr/*' 'deps-build/*' 'lib/cJSON.c'
genhtml ./code_coverage/client_coverage.info --output-directory ./code/coverage/client_coverage_report

