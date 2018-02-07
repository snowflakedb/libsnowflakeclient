::
:: Build Snowflake Client library
::
rmdir /q /s cmake-build
mkdir cmake-build
cd cmake-build
cmake -G "Visual Studio 14 2015" -A x64 ..
cmake --build . --clean-first --config Release
cd ..
