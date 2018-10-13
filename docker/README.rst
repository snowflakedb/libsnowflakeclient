********************************************************************************
Docker images
********************************************************************************

This directory includes docker files to for Docker images.

Build Docker images for tests
======================================================================

See test-base directory.

Testing Code
======================================================================

Before running a container
----------------------------------------------------------------------

Set the Snowflake connection info in ``parameters.json`` and place it in ``libsnowflakeclient`` directory:

.. code-block:: json

    {
        "testconnection": {
            "SNOWFLAKE_TEST_USER":      "<your_user>",
            "SNOWFLAKE_TEST_PASSWORD":  "<your_password>",
            "SNOWFLAKE_TEST_ACCOUNT":   "<your_account>",
            "SNOWFLAKE_TEST_WAREHOUSE": "<your_warehouse>",
            "SNOWFLAKE_TEST_DATABASE":  "<your_database>",
            "SNOWFLAKE_TEST_SCHEMA":    "<your_schema>",
            "SNOWFLAKE_TEST_ROLE":      "<your_role>"
        }
    }

