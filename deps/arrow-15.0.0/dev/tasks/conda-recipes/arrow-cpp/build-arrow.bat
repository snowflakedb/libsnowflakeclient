@echo on

mkdir cpp\build
pushd cpp\build

:: Enable CUDA support
if "%cuda_compiler_version%"=="None" (
    set "EXTRA_CMAKE_ARGS=-DARROW_CUDA=OFF"
) else (
    set "EXTRA_CMAKE_ARGS=-DARROW_CUDA=ON"
)

:: # reusable variable for dependencies we cannot yet unvendor
set "READ_RECIPE_META_YAML_WHY_NOT=OFF"

:: for available switches see
:: https://github.com/apache/arrow/blame/apache-arrow-12.0.0/cpp/cmake_modules/DefineOptions.cmake
cmake -G "Ninja" ^
      -DARROW_ACERO=ON ^
      -DARROW_BOOST_USE_SHARED:BOOL=ON ^
      -DARROW_BUILD_STATIC:BOOL=OFF ^
      -DARROW_BUILD_TESTS:BOOL=OFF ^
      -DARROW_BUILD_UTILITIES:BOOL=OFF ^
      -DARROW_COMPUTE:BOOL=ON ^
      -DARROW_CSV:BOOL=ON ^
      -DARROW_DATASET:BOOL=ON ^
      -DARROW_DEPENDENCY_SOURCE=SYSTEM ^
      -DARROW_FILESYSTEM:BOOL=ON ^
      -DARROW_FLIGHT:BOOL=ON ^
      -DARROW_FLIGHT_REQUIRE_TLSCREDENTIALSOPTIONS:BOOL=ON ^
      -DARROW_FLIGHT_SQL:BOOL=ON ^
      -DARROW_GANDIVA:BOOL=ON ^
      -DARROW_GCS:BOOL=ON ^
      -DARROW_HDFS:BOOL=ON ^
      -DARROW_JSON:BOOL=ON ^
      -DARROW_MIMALLOC:BOOL=ON ^
      -DARROW_ORC:BOOL=ON ^
      -DARROW_PACKAGE_PREFIX="%LIBRARY_PREFIX%" ^
      -DARROW_PARQUET:BOOL=ON ^
      -DARROW_S3:BOOL=ON ^
      -DARROW_SIMD_LEVEL:STRING=NONE ^
      -DARROW_SUBSTRAIT:BOOL=ON ^
      -DARROW_USE_GLOG:BOOL=ON ^
      -DARROW_WITH_BROTLI:BOOL=ON ^
      -DARROW_WITH_BZ2:BOOL=ON ^
      -DARROW_WITH_LZ4:BOOL=ON ^
      -DARROW_WITH_NLOHMANN_JSON:BOOL=ON ^
      -DARROW_WITH_OPENTELEMETRY:BOOL=%READ_RECIPE_META_YAML_WHY_NOT% ^
      -DARROW_WITH_SNAPPY:BOOL=ON ^
      -DARROW_WITH_ZLIB:BOOL=ON ^
      -DARROW_WITH_ZSTD:BOOL=ON ^
      -DBUILD_SHARED_LIBS=ON ^
      -DBoost_NO_BOOST_CMAKE=ON ^
      -DCMAKE_BUILD_TYPE=release ^
      -DCMAKE_CXX_STANDARD=17 ^
      -DCMAKE_INSTALL_PREFIX="%LIBRARY_PREFIX%" ^
      -DCMAKE_UNITY_BUILD=ON ^
      -DLLVM_TOOLS_BINARY_DIR="%LIBRARY_BIN%" ^
      -DPARQUET_REQUIRE_ENCRYPTION:BOOL=ON ^
      -DPython3_EXECUTABLE="%PYTHON%" ^
      %EXTRA_CMAKE_ARGS% ^
      ..
if %ERRORLEVEL% neq 0 exit 1

cmake --build . --target install --config Release
if %ERRORLEVEL% neq 0 exit 1

popd

:: clean up between builds (and to save space)
rmdir /s /q cpp\build
