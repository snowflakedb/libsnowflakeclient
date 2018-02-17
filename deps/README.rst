Build Dependent Libraries
======================================================================

Prerequiste
----------------------------------------------------------------------

All platform require:

- gcc or equivalent compiler and linkers
- cmake 2.8 or later

Windows requires the following additional tools

- nsam (http://www.nasm.us/)
- perl (http://strawberryperl.com/)

Build
----------------------------------------------------------------------

Linux and OSX
^^^^^^^^^^^^^

.. code-block:: bash

    ./scripts/build_dependencies.sh -t Release

Windows
^^^^^^^^^^

Add x64 or x86 for 64 bit or 32 bit Windows:

.. code-block:: bash

    .\scripts\build_zlib.sh x64 Release
    .\scripts\build_openssl.sh x64 Release
    .\scripts\build_curl.sh x64 Release

or

.. code-block:: bash

    .\scripts\build_zlib.sh x86 Release
    .\scripts\build_openssl.sh x86 Release
    .\scripts\build_curl.sh x86 Release
