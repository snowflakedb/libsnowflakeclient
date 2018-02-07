::
:: Build zlib for Windows
::
ZLIB_DIR=zlib-1.2.11
cd deps\%ZLIB_DIR%
rmdir /S /Q cmake-build
mkdir cmake-build
cd cmake-build
cmake -G "Visual Studio 14 2015" -A x64 ..
cmake --build . --clean-first --config Release
cd ..\..
cd deps-build\win64
rmdir /S /Q zlib
mkdir zlib
cd zlib
mkdir include
mkdir lib
cp ..\..\
:: TODO: copy zlib.lib and headers to a staging area for curl