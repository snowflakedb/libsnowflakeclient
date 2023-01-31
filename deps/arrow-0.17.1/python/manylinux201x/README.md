<!---
  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing,
  software distributed under the License is distributed on an
  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
  KIND, either express or implied.  See the License for the
  specific language governing permissions and limitations
  under the License.
-->

## Manylinux201x wheels for Apache Arrow

This folder provides base Docker images and an infrastructure to build
`manylinux2010` and `manylinux2014` compatible Python wheels that should be installable on all
Linux distributions published in last several years.

The process is split up in two parts:

1. There are base Docker images that contain the build dependencies for
   Arrow.  Those images do not need to be rebuilt frequently, and are hosted
   on the public Docker Hub service.

2. Based on on these images, there is a bash script (`build_arrow.sh`) that
   the PyArrow wheels for all supported Python versions, and place them
   in the `dist` folder.

### Building PyArrow

You can build the PyArrow wheels by running the following command in this
directory (this is for Python 3.7, similarly you can pass another value
for `PYTHON_VERSION`):

```bash
# Build the python packages
docker-compose run -e PYTHON_VERSION="3.7" centos-python-manylinux2010
# Now the new packages are located in the dist/ folder
ls -l dist/
```

You can do the same for `manylinux2014` by substituting `centos-python-manylinux2014`.

### Re-building the build image

In case you want to make changes to the base Docker image (for example because
you want to update a dependency to a new version), you must re-build it.
The Docker configuration is in `Dockerfile-x86_64_base_2010` and `Dockerfile-x86_64_base_2014`,
and it calls into scripts stored under the `scripts` directory.

```bash
docker-compose build centos-python-manylinux2010
```

For each dependency, a bash script in the `scripts/` directory downloads the
source code, builds it and installs the build results.  At the end of each
dependency build the sources are removed again so that only the binary
installation of a dependency is persisted in the Docker image.

### Publishing a new build image

If you have write access to the Docker Hub Ursa Labs account, you can directly
publish a build image that you built locally.

```bash
$ docker push centos-python-manylinux2010
The push refers to repository [arrowdev/arrow_manylinux2010_x86_64_base]
a1ab88d27acc: Pushing [==============>                                    ]  492.5MB/1.645GB
[... etc. ...]
```
