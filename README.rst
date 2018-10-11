********************************************************************************
Snowflake Connector for C/C++
********************************************************************************

.. image:: https://travis-ci.org/snowflakedb/libsnowflakeclient.svg?branch=master
    :target: https://travis-ci.org/snowflakedb/libsnowflakeclient

.. image:: https://codecov.io/gh/snowflakedb/libsnowflakeclient/branch/master/graph/badge.svg
    :target: https://codecov.io/gh/snowflakedb/libsnowflakeclient

.. image:: http://img.shields.io/:license-Apache%202-brightgreen.svg
    :target: http://www.apache.org/licenses/LICENSE-2.0.txt

.. image:: https://ci.appveyor.com/api/projects/status/i1rkda42xeg2bodv/branch/master?svg=true
    :target: https://ci.appveyor.com/project/smtakeda/libsnowflakeclient/branch/master

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

Add x64 or x86 for 64 bit or 32 bit Windows:

.. code-block:: bash

    .\scripts\build_libsnowflakeclient.bat x64 Release

or

.. code-block:: bash

    .\scripts\build_libsnowflakeclient.bat x86 Release

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

Add x64 or x86 for 64 bit or 32 bit Windows:

.. code-block:: bash

    ./scripts/run_tests.bat x64 Release

or

.. code-block:: bash

    ./scripts/run_tests.bat x86 Release

	
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
