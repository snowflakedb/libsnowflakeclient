# Azure Storage C++ Client Library (Lite)

## About

The Azure Storage Client Library (Lite) for C++ allows you to build applications against Microsoft Azure Storage's blob service. This is a minimum dependency version that provide basic object storage. For an overview of Azure Storage, see [Introduction to Microsoft Azure Storage](http://azure.microsoft.com/en-us/documentation/articles/storage-introduction/).
If you want to use other services of Azure Storage, or a more comprehensive functionality of Blob service, please see [Azure Storage C++ Client Library](https://github.com/azure/azure-storage-cpp).

## Features
The full supported Azure Storage API can be found in the following list, please be aware that only part of the functionality of some APIs are supported:
- [List Containers](https://docs.microsoft.com/en-us/rest/api/storageservices/list-containers2).
- [Create Container](https://docs.microsoft.com/en-us/rest/api/storageservices/create-container).
- [Get Container Properties](https://docs.microsoft.com/en-us/rest/api/storageservices/get-container-properties).
- [Delete Container](https://docs.microsoft.com/en-us/rest/api/storageservices/delete-container).
- [List Blobs](https://docs.microsoft.com/en-us/rest/api/storageservices/list-blobs).
- [Put Blob](https://docs.microsoft.com/en-us/rest/api/storageservices/put-blob).
- [Get Blob](https://docs.microsoft.com/en-us/rest/api/storageservices/get-blob).
- [Get Blob Properties](https://docs.microsoft.com/en-us/rest/api/storageservices/get-blob-properties).
- [Delete Blob](https://docs.microsoft.com/en-us/rest/api/storageservices/delete-blob).
- [Copy Blob](https://docs.microsoft.com/en-us/rest/api/storageservices/copy-blob).
- [Put Block](https://docs.microsoft.com/en-us/rest/api/storageservices/put-block).
- [Put Block List](https://docs.microsoft.com/en-us/rest/api/storageservices/put-block-list).
- [Get Block List](https://docs.microsoft.com/en-us/rest/api/storageservices/get-block-list).
- [Put Page](https://docs.microsoft.com/en-us/rest/api/storageservices/put-page).
- [Get Page Ranges](https://docs.microsoft.com/en-us/rest/api/storageservices/get-page-ranges).
- [Append Block](https://docs.microsoft.com/en-us/rest/api/storageservices/append-block).

## Installation
### Supported Platforms
Please be aware that below platforms are tested and verified, but other platforms beyond this list can be working with some modification on the build steps resolving the dependencies. Feel free to test them out and contribute back.
  - Ubuntu 16.04 x86_64.
  - Red Hat 6 x86_64.
  - Red Hat 7 x86_64.
  - MacOS Sierra version 10.12.6 building with x86_64 or i386.
  - Windows 10 with Visual Studio 2017, x86 or x64.

### Build this library on Linux
Project dependencies:
  - GNUTLS(or Openssl v1.0.1)
  - libcurl v7.35.0
  - CMake v2.8.12.2
  - g++ v4.8.2
  - UUID 2.13.1-0.4
#### Clone the latest code from this repository:
```
git clone https://github.com/azure/azure-storage-cpplite.git
```
#### Install the dependencies, e.g. on Ubuntu:
```
sudo apt-get install libssl-dev libcurl4-openssl-dev cmake g++ uuid-dev
```
Or, on Red Hat OS:
```
sudo yum install openssl-devel.x86_64 libcurl-devel.x86_64 cmake.x86_64 gcc-c++.x86_64 uuid-devel
```
Please be aware that RHEL6 comes with gcc version 4.4.7, which does not meet the requirement of this SDK. In order to use this SDK, [devtoolset](http://linux.web.cern.ch/linux/devtoolset/#install) needs to be installed properly.
Please be aware that on some Linux distributions, pkg-config is not properly installed that would result in CMake to not behave as expected. Installing pkg-config or updating it will eliminate the potential issue. The tested version of pkg-config is 0.29.1-0.
#### Build and install azure-storage-cpplite:
```
cd azure-storage-cpplite
mkdir build.release
cd build.release
CXX=g++ cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_OPENSSL=true
sudo make install
```
#### Use GNUTLS instead of OpenSSL:
Alternatively, you can use GNUTLS instead of OpenSSL. Simply install GNUTLS and remove the argument `-DUSE_OPENSSL` during build.

### Build this library on MacOS
Project dependencies:
-   Openssl v1.0.1 to v 1.1.1
-   libcurl
-   CMake
-   Clang (Comes with XCode)

#### Clone the latest code from this repository:
```
git clone https://github.com/azure/azure-storage-cpplite.git
```
#### Install dependencies:
```
Brew install openssl curl-openssl cmake
```
#### Build and install azure-storage-cpplite:
```
cd azure-storage-cpplite
mkdir build.release
cd build.release
cmake .. -DCMAKE_BUILD_TYPE=Release
sudo make install
```
#### Specifying customized OpenSSL or libcurl root folder:
Default OpenSSL or libcurl root directory may not be applicable for all users. In that case, following parameters can be used to specify the prefered path:
`-DCURL_ROOT_DIR=<curl's root directory>`.
`-DOPENSSL_ROOT_DIR=<openssl's root directory>`.
#### Specifying the build for x86_64 or i386:
First, follow [OpenSSL build guide](https://wiki.openssl.org/index.php/Compilation_and_Installation#OS_X) to build and install x86_64 or i386 version of OpenSSL.
Then, follow [CURL build guide](https://curl.haxx.se/docs/install.html) to build and install x86_64 or i386 version of libcurl.
At last, when building this library, add argument below when executing CMake commands:
`-DCMAKE_OSX_ARCHITECTURES=x86_64` or `i386`.

### Build this library on Windows
Project dependencies:
  - Openssl v1.0.2o
  - libcurl v7.60.0
  - CMake v2.8.12.2
  - Visual Studio 2017
  - Perl (If OpenSSL needs to be built manually)
#### Clone the latest code from this repository:
```
git clone https://github.com/azure/azure-storage-cpplite.git
```
#### Prepare and build the dependencies.
There are two major dependencies on Windows: libcurl and OpenSSL. You can reference the how-tos in this readme or install your own choice of pre-built binaries.
##### How to build libcurl and openssl?
You can use the following guide to build libcurl and openssl on your own. Note that Perl must be installed to build OpenSSL.
Clone the source code of libcurl and OpenSSL:
```
git clone --branch curl-7_60_0 https://github.com/curl/curl.git
git clone --branch OpenSSL_1_0_2o https://github.com/openssl/openssl.git
```

Build OpenSSL first, e.g. if you want to build x86-debug version, note that the perlpath should change to your systems perl install path:
```
.\curl\projects\build-openssl.bat vc14.1 x64 debug .\openssl\ -perlpath C:\Strawberry
```

Then build libcurl:
Run the setup batch file
```
.\curl\buildconf.bat
```
Modify .\curl\winbuild\MakefileBuild.vc to support linking with different configuration type.
Change line:
```
DEVEL_LIB = $(WITH_DEVEL)/lib
```
To:
```
!IF "$(DEBUG)"=="yes"
DEVEL_LIB    = $(WITH_DEVEL)/lib/Debug
!ELSE
DEVEL_LIB    = $(WITH_DEVEL)/lib/Release
!ENDIF
```
Create `.\curl\deps` folder.
Add `include`, `lib\Debug` and `lib\Release` to `.\curl\deps` folder.
Copy OpenSSL's include and built libraries into created `.\curl\deps` folder:
  1. Create `openssl` folder in `.curl\deps\include`
  2. Copy `.\openssl\inc32\openssl` into `.\curl\deps\include\openssl`, and you will see `.\curl\deps\include\openssl\openssl`. Please note the duplicated `openssl` in path is somehow necessary for curl build-script.
  3. Copy `.\openssl\build\Win32\VC14.1\LIB Release\*.lib` to `.\curl\deps\lib\Release` folder if static libraries are built, otherwise copy the `*.dll` instead.
Open Visual Studio CMD tool, navigate to `.\curl\winbuild` and start building, e.g. Run 'x64 Native Tools command Prompt for VS2017.exe' and run:
```
nmake /f Makefile.vc mode=static VC=15 MACHINE=x64 WITH_SSL=static DEBUG=yes WITH_DEVEL=..\deps RTLIBCFG=static
```
#### Build azure-storage-cpplite using CMake in command line
Note that the `<path to OpenSSL>` should be the absolute path constructed in curl directory as dependency when building curl, normally it should be `<CURL_ROOT_PATH>\deps`. Also, `<CURL_ROOT_PATH>` should be absolute path. Using relative path can cause build to fail.
```
cd azure-storage-cpplite
mkdir build.release
cd build.release
cmake .. -DCURL_ROOT_DIR="<path to curl>" -DOPENSSL_ROOT_DIR="<path to OpenSSL>"
MSBuild.exe azurestoragelite.sln /p:Configuration=Release
```
If you have libcurl and OpenSSL built or installed by yourself with another approach, you can specify the path to the library and headers of them instead:
```
cmake .. -DCURL_LIBRARIES="<path to curl libraries>" -DCURL_INCLUDE_DIRS="<path to curl include folder>" -DOPENSSL_SSL_LIBRARY="<path to OpenSSL's ssl library>" -DOPENSSL_CRYPTO_LIBRARY="<path to OpenSSL's crypto library>" -DOPENSSL_INCLUDE_DIR="<path to OpenSSL's include folder>"
MSBuild.exe azurestoragelite.sln /p:Configuration=Release
```
There are some advanced options to config the build when using cmake commands:
`-DBUILD_TESTS` : specify `true` or `false` to control if tests should be built.
`-DBUILD_SAMPLES` : specify `true` or `false` to control if samples should be built.
`-DOPENSSL_MSVC_STATIC_RT` : specify `true` or `false` to control if the library is building into static library. Note this should be used together with `-DBUILD_SHARED_LIBS=` either `ON` or `OFF`.
`-DBUILD_WITH_MT` : specify `true` or `false` to control if the library is using `/MT` or `/MTd`.
`-DOPENSSL_LINK_TYPE` : specify `static` or `dll` to control if OpenSSL should be linked statically.
`-DCURL_LINK_TYPE` : specify `static` or `dll` to control if libcurl should be linked statically.
`-DCMAKE_GENERATOR_PLATFORM` : specify `x86` or `x64` to config the generator platform type.
`-DCMAKE_BUILD_TYPE` : specify `Debug` or `Release` to config the build type.
#### Build azure-storage-cpplite using Visual Studio 2017
Use Visual Studio 2017 to open the folder that contains this repository. Modify the `environments` in `CMakeSettings.json`. E.g.:
```
  "environments": [
    {
      "curlRootDir": "E:\\dev\\src\\cpp\\curl",
      "opensslRootDir": "E:\\dev\\src\\cpp\\curl\\deps",
      "buildWithMT": "true", //true or false
      "buildSharedLibrary": "false", //true or false
      "buildTests": "true", //true or false
      "buildSamples": "true", //true or false
      "opensslLinkType": "static", //static or dll
      "curlLinkType": "static" //static or dll
    }
  ]
```

#### Build dependencies with /MD (/MDd) options instead.
If need to build /MD(/MDd) version, there are two things to do when building OpenSSL:
  1. Modify .\openssl\ms\nt.mak, change CFLAG's from `/MT` to `/MD`
  2. Comment out this line in .\curl\projects\build-openssl.bat: `perl Configure debug-VC-WIN32 no-asm --prefix=%CD%`
When building libcurl, remove the trailing `RTLIBCFG=static` option will build with `/MD` or `/MDd`

### Build static library instead of dynamic libraries.
By default, dynamic libraries are built. Adding `-DBUILD_SHARED_LIBS=OFF` will build static library instead.

## Usage
Simply include the header files after installing the library, everything is good to go. For a more comprehensive sample, please see [sample](https://github.com/azure/azure-storage-cpplite/blob/master/sample/sample.cpp).
To build the sample, add `-DBUILD_SAMPLES=true` when building the repository.
```
#include "storage_credential.h"
#include "storage_account.h"
#include "blob/blob_client.h"

// Your settings
std::string account_name = "YOUR_ACCOUNT_NAME";
std::string account_key = "YOUR_ACCOUNT_KEY";
bool use_https = true;
std::string blob_endpoint = "CUSTOMIZED_BLOB_ENDPOINT";
int connection_count = 2;

// Setup the client
azure::storage_lite::shared_key_credential credential(account_name, account_key);
azure::storage_lite::storage_account(account_name, credential, use_https, blob_endpoint);
azure::storage_lite::blob_client client(storage_account, connection_count);

// Start using
auto outcome = client.create_container("YOUR_CONTAINER_NAME").get();
```
## License
This project is licensed under MIT.
 
## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repositories using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

When contributing to this client library, there are following ground rules:
1. All source code change should be clearly addressing issues or adding new features, and should be covered with test.
2. Coding style should match with the existing code.
3. Any contribution should not degrade performance or complex functionality.
4. Introducing new dependency should be done with much great caution. Any new dependency should introduce significant performance improvement or unblock critical user scenario.

### Build Test
- Dev dependency: catch2.
Download [Catch2 single header version](https://raw.githubusercontent.com/catchorg/Catch2/master/single_include/catch2/catch.hpp) and put it in the folder `.\test\catch2\`.
Add `-DBUILD_TESTS=true` when building the repository.
Please modify the [connection string here](https://github.com/azure/azure-storage-cpplite/blob/master/test/test_base.h#L18) in `.\test\test_base.h` line 18 `static std::string sscs = "DefaultEndpointsProtocol=https;";` to successfully run the tests. All the tests use standard Azure Storage account.
#### *Please note that in order to run test, a minimum version of g++ 5.1 is required on Linux, and Visual Studio 2017 is required on Windows.*
