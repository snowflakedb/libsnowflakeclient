@rem Licensed to the Apache Software Foundation (ASF) under one
@rem or more contributor license agreements.  See the NOTICE file
@rem distributed with this work for additional information
@rem regarding copyright ownership.  The ASF licenses this file
@rem to you under the Apache License, Version 2.0 (the
@rem "License"); you may not use this file except in compliance
@rem with the License.  You may obtain a copy of the License at
@rem
@rem   http://www.apache.org/licenses/LICENSE-2.0
@rem
@rem Unless required by applicable law or agreed to in writing,
@rem software distributed under the License is distributed on an
@rem "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
@rem KIND, either express or implied.  See the License for the
@rem specific language governing permissions and limitations
@rem under the License.

@echo on

@rem Avoid picking up AppVeyor-installed OpenSSL (linker errors with gRPC)
@rem XXX Perhaps there is a smarter way of solving this issue?
rd /s /q C:\OpenSSL-Win32
rd /s /q C:\OpenSSL-Win64
rd /s /q C:\OpenSSL-v11-Win32
rd /s /q C:\OpenSSL-v11-Win64
rd /s /q C:\OpenSSL-v111-Win32
rd /s /q C:\OpenSSL-v111-Win64

conda config --set auto_update_conda false
conda info -a

conda config --set show_channel_urls True

@rem Help with SSL timeouts to S3
conda config --set remote_connect_timeout_secs 12

conda info -a

if "%GENERATOR%"=="Ninja" set need_vcvarsall=1

if defined need_vcvarsall (
    @rem Select desired compiler version
    if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2017" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
    ) else (
        call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
    )
)

if "%GENERATOR%"=="Ninja" conda install -y -q ninja

if "%USE_CLCACHE%" == "true" (
    @rem Use clcache for faster builds
    pip install -q git+https://github.com/frerich/clcache.git
    @rem Limit cache size to 500 MB
    clcache -M 500000000
    clcache -c
    clcache -s
    powershell.exe -Command "Start-Process clcache-server"
)

if "%ARROW_S3%" == "ON" (
    @rem Download Minio somewhere on PATH, for unit tests
    appveyor DownloadFile https://dl.min.io/server/minio/release/windows-amd64/minio.exe -FileName C:\Windows\Minio.exe || exit /B
)
