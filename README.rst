********************************************************************************
Snowflake Connector for C/C++
********************************************************************************

.. image:: https://github.com/snowflakedb/libsnowflakeclient/workflows/Build%20and%20Test/badge.svg?branch=master
    :target: https://github.com/snowflakedb/libsnowflakeclient/actions?query=workflow%3A%22Build+and+Test%22+branch%3Amaster

.. image:: https://codecov.io/github/snowflakedb/libsnowflakeclient/coverage.svg?branch=master
    :target: https://codecov.io/github/snowflakedb/libsnowflakeclient?branch=master

.. image:: http://img.shields.io/:license-Apache%202-brightgreen.svg
    :target: http://www.apache.org/licenses/LICENSE-2.0.txt


Prerequisites
================================================================================

Operating system
For a list of the operating systems supported by Snowflake clients, see `Operating system support <https://docs.snowflake.com/en/release-notes/requirements#label-client-operating-system-support>`_.

To build libsnowflakeclient, the following software must be installed:

- On Windows: Visual Studio 2015, 2017, 2019 or 2022
- On Linux:

  - gcc/g++ 8.3 or higher. **Note**: on certain OS (e.g. Centos 7) the preinstalled gcc/libstdc++ version is below the required minimum. For Centos 7, this is 4.8.5, which is below the requirement. Building libsnowflakeclient might be unsuccessful on such OS's until the prerequisite is fulfilled, i.e. libraries upgraded to at least the minimum version.
  - cmake 3.17 or higher

- On macOS:

  - clang
  - cmake 3.17 or higher

To run test cases, the following software must be installed:

- jq: https://jqlang.github.io/jq/download/
- python 3.7 or higher

Build and Tests
======================================================================

Build
----------------------------------------------------------------------

Linux and OSX
^^^^^^^^^^^^^

.. code-block:: bash

    ./scripts/build_dependencies.sh
    ./scripts/build_libsnowflakeclient.sh

Windows
^^^^^^^^^^
Set environment variables: PLATFORM: [x64, x86], BUILD_TYPE: [Debug, Release], VS_VERSION: [VS14, VS15, VS16, VS17] and run the script.

.. code-block:: bash

    set platform=x64
    set build_type=Release
    set vs_version=VS17

    .\scripts\build_dependencies.bat
    .\scripts\build_libsnowflakeclient.bat

Prepare for Test
----------------------------------------------------------------------

Set the Snowflake connection info in ``parameters.json`` and place it in the root path of libsnowflakeclient repository:

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
            "SNOWFLAKE_TEST_HOST":      "<your_snowflake_url>"
            "CLOUD_PROVIDER":           "<your_cloud_provider>"
        }
    }

where:

- :code:`<your_snowflake_url>` is optional. Set it when your Snowflake URL is not in the format of :code:`account.snowflakecomputing.com`.
- :code:`<CLOUD_PROVIDER>` is the cloud platform of your Snowflake account. (:code:`AWS`, :code:`AZURE` or :code:`GCP`).

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

Set environment variables: PLATFORM: [x64, x86], BUILD_TYPE: [Debug, Release], VS_VERSION: [VS14, VS15, VS16, VS17] and run the script.

.. code-block:: bash

    set platform=x64
    set build_type=Release
    set vs_version=VS17

   .\scripts\run_tests.bat

	
Code Coverage (Linux)
----------------------------------------------------------------------

Ensure you have lcov 1.11 or later version and have account on AWS and AZURE for test.

- Modify ``script/gen_coverage_report.sh`` to add test account information there, not only the AWS information at the top, but also AZURE information at the bottom.
- run gen_coverage_report.sh to generate code coverage report
.. code-block:: bash

    ./scripts/gen_coverage_report.sh

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

Note
===============

This driver currently does not support GCP regional endpoints. Please ensure that any workloads using through this driver do not require support for regional endpoints on GCP. If you have questions about this, please contact Snowflake Support.

