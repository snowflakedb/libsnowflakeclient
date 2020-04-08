*************************************************
Develop and Debug by CLion and Docker
*************************************************

This memo explains how to set up the dev environment for LibSnowflakeClient using CLion and Docker on Linux.

Prerequisite
^^^^^^^^^^^^

- CLion
- Docker
- add the following line to /etc/hosts

.. code-block:: bash

    127.0.0.1 dev.docker

Configurations
^^^^^^^^^^^^^^

- Docker host name: dev.docker
- SSH Port mapping: Host 7786 to Container 22
- GDB Port mapping: Host 7787 to Container 7777

Setup
^^^^^^^^

- Open a terminal and go to ``libsnowflakeclient`` git directory and run ``ci/dev/start.sh``. Answer to the password for ``debugger@dev.docker`` to ``pwd``.
  This script also adds the public key to the container.

    .. code-block:: bash

        $ ./ci/dev/start.sh

    Type `pwd` if the password is asked.

- Ensure the docker container is up and running.

    .. code-block:: bash

        $ docker ps | grep clion_libsnowflake
        ...        clion_libsnowflakeclient_dev_image   ...     Up 49 minutes       0.0.0.0:7786->22/tcp, 0.0.0.0:7787->7777/tcp   clion_libsnowflakeclient_dev

- Use CLion to open the ``libsnowflakeclient`` project.

- In ``File->Settings->Build, Execution, Deployment-> Toolchains``, create a new toolchain:

    - Name: LibSnowflakeClient Container
    - Credentials: ssh://debugger@dev.docker:7776
    - CMake: cmake3
    - Make: make
    - C Compiler: gcc52
    - C++ Compiler: g++52
    - Debugger: /opt/rh/devtooset-4/root/usr/bin/gdb

    .. note::

        This configuration is used internally and subject to change.

- In ``File->Settings->Build, Execution, Deployment->CMake``, create a new profile with the toolchain:

    - Name: Debug-Container
    - Build Type: Debug
    - Toolchain: LibSnowflakeClient Container

- In ``File->Settings->Build, Execution, Deployment->Deployment``, click Mappings of ``LibSnowflakeClient Container`` toolchain

    - Deployment Path: /home/debugger/libsnowflakeclient
    - Web path: (blank)

    .. note::

        - Don't use the default /tmp as one without root power cannot operate on it.
        - Don't change the root path in the Connection tab. Leave / as is and set the Deployment Path
          to /home/debugger/libsnowflakeclient

- Select ``Tools->CMake->Reset Cache and Reload Project``. This will upload the files from the host to the container.
  But you will see errors because the dependent components are missing.
- Login the container.

    .. code-block:: bash

        ssh -p 7786 debugger@dev.docker

- Download or build dependencies in the container.

    .. code-block:: bash

        cd /home/debugger/libsnowflakeclient
        source ./ci/dev/docker_init.sh
        ./ci/build/build.sh

- Select ``Tools->CMake->Reload CMake Project`` to reload the CMakeList.txt. You should see all dependent components
  are ready.

- Done


Build and Run Tests
^^^^^^^^^^^^^^^^^^^
- Choose ``snowflakeclient`` in the configuration menu and click ``Build`` icon.
- Set the environment variables to CMake Application template and all tests if they are missing.

.. code-block:: text

    SNOWFLAKE_TEST_HOST=HOST
    SNOWFLAKE_TEST_USER=USER
    SNOWFLAKE_TEST_PASSWORD=PASSWORD;
    SNOWFLAKE_TEST_ACCOUNT=ACCOUNT
    SNOWFLAKE_TEST_DATABASE=DATABASE
    SNOWFLAKE_TEST_SCHEMA=SCHEMA
    SNOWFLAKE_TEST_WAREHOUSE=WAREHOUSE
    SNOWFLAKE_TEST_ROLE=ROLE

- Choose the test name and click ``Run`` or ``Debug`` icon.
