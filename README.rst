********************************************************************************
Snowflake Connector for C/C++
********************************************************************************

.. image:: https://github.com/snowflakedb/libsnowflakeclient/workflows/Build%20and%20Test/badge.svg?branch=master
    :target: https://github.com/snowflakedb/libsnowflakeclient/actions?query=workflow%3A%22Build+and+Test%22+branch%3Amaster

.. image:: http://img.shields.io/:license-Apache%202-brightgreen.svg
    :target: http://www.apache.org/licenses/LICENSE-2.0.txt

*Under development. No functionality works. Suggestion is welcome at any time.*

Build and Tests
======================================================================

Build
----------------------------------------------------------------------

Ensure you have cmake 2.8 or later version.

Linux and OSX
^^^^^^^^^^^^^

.. code-block:: bash

    ./scripts/build_libsnowflakeclient.sh

Windows
^^^^^^^^^^

Set environment variables: PLATFORM: [x64, x86], BUILD_TYPE: [Debug, Release], VS_VERSION: [VS14, VS15, VS16] and run the script.

.. code-block:: bash

    set platform=x64
    set build_type=Debug
    set vs_version=VS14
    ci\build.bat

Prepare for Test
----------------------------------------------------------------------

Set the Snowflake connection info in ``parameters.json`` and place it in $HOME:

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

Proxy
^^^^^^^^^^

Libsnowflakeclient supports HTTP and HTTPS proxy connections using environment variables. To use a proxy server configure the following environment variables:

- http_proxy
- https_proxy
- no_proxy

.. code-block:: bash

    export http_proxy="[protocol://][user:password@]machine[:port]"
    export https_proxy="[protocol://][user:password@]machine[:port]"

More info can be found on the `libcurl tutorial`__ page.

.. __: https://curl.haxx.se/libcurl/c/libcurl-tutorial.html#Proxies

Run Tests
----------------------------------------------------------------------

Run the tests. The test parameter environment variables will be set automatically.

Linux and OSX
^^^^^^^^^^^^^

.. code-block:: bash

    ./scripts/run_tests.sh

Windows
^^^^^^^^^^

Set environment variables: PLATFORM: [x64, x86], BUILD_TYPE: [Debug, Release], VS_VERSION: [VS14, VS15, VS16] and run the script.

.. code-block:: bash

    set platform=x64
    set build_type=Debug
    set vs_version=VS14
    ci\test.bat

	
Profiling (Linux and OSX)
----------------------------------------------------------------------

If you want to use ``gprof``, add ``-p`` option to the build script, run a test program followed by ``gprof``, for example:

.. code-block:: bash

    ./scripts/build_libsnowflakeclient.sh -p
    ./cmake-build/examples/ex_connect
    gprof ./cmake-build/examples/ex_connect gmon.out

Check memory leak by Valgrind (Linux)
----------------------------------------------------------------------

Use ``valgrind`` to check memory leak.

.. code-block:: bash

    ./scripts/build_libsnowflakeclient.sh
    valgrind --leak-check=full ./cmake-build/examples/ex_connect

and verify no error in the output:

.. code-block:: bash

     ERROR SUMMARY: 0 errors from 0 contexts ...
