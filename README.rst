********************************************************************************
Snowflake Connector for C/C++
********************************************************************************

.. image:: https://travis-ci.org/snowflakedb/libsnowflakeclient.svg?branch=master
    :target: https://travis-ci.org/snowflakedb/libsnowflakeclient

.. image:: https://codecov.io/gh/snowflakedb/libsnowflakeclient/branch/master/graph/badge.svg
    :target: https://codecov.io/gh/snowflakedb/libsnowflakeclient

.. image:: http://img.shields.io/:license-Apache%202-brightgreen.svg
    :target: http://www.apache.org/licenses/LICENSE-2.0.txt

*Under development. No functionality works. Suggestion is welcome at any time.*

Build and Tests
================================================================================

Build
----------------------------------------------------------------------
Ensure you have cmake 2.8 or later version.

.. code-block:: bash

    mkdir cmake-build
    cd cmake-build
    cmake .. -G"Unix Makefiles"
    make

Test
----------------------------------------------------------------------

Run the tests. The test parameter environment variables will be set automatically.

.. code-block:: bash

    ./scripts/run_tests.sh

Profile
----------------------------------------------------------------------

If you want to use ``gprof``, add ``-p`` option to the build script, run a test program followed by ``gprof``, for example:

.. code-block:: bash

    ./scripts/build_libsnowflakeclient.sh -p
    ./cmake-build/examples/ex_connect
    gprof ./cmake-build/examples/ex_connect gmon.out

Check memory leak by Valgrind
----------------------------------------------------------------------

Use ``valgrind`` to check memory leak.

.. code-block:: bash

    ./scripts/build_libsnowflakeclient.sh
    valgrind --leak-check=full ./cmake-build/examples/ex_connect

and verify no error in the output:

.. code-block:: bash

     ERROR SUMMARY: 0 errors from 0 contexts ...
