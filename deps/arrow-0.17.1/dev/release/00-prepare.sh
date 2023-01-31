#!/bin/bash
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
set -ue

SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <version> <next_version>"
  exit 1
fi

update_versions() {
  local base_version=$1
  local next_version=$2
  local type=$3

  case ${type} in
    release)
      local version=${base_version}
      local r_version=${base_version}
      ;;
    snapshot)
      local version=${next_version}-SNAPSHOT
      local r_version=${base_version}.9000
      ;;
  esac

  cd "${SOURCE_DIR}/../../c_glib"
  sed -i.bak -E -e \
    "s/^m4_define\(\[arrow_glib_version\], .+\)/m4_define([arrow_glib_version], ${version})/" \
    configure.ac
  sed -i.bak -E -e \
    "s/^version = '.+'/version = '${version}'/" \
    meson.build
  rm -f configure.ac.bak meson.build.bak
  git add configure.ac meson.build
  cd -

  cd "${SOURCE_DIR}/../../ci/scripts"
  sed -i.bak -E -e \
    "s/^pkgver=.+/pkgver=${r_version}/" \
    PKGBUILD
  rm -f PKGBUILD.bak
  git add PKGBUILD
  cd -

  cd "${SOURCE_DIR}/../../cpp"
  sed -i.bak -E -e \
    "s/^set\(ARROW_VERSION \".+\"\)/set(ARROW_VERSION \"${version}\")/" \
    CMakeLists.txt
  rm -f CMakeLists.txt.bak
  git add CMakeLists.txt
  cd -

  cd "${SOURCE_DIR}/../../csharp"
  sed -i.bak -E -e \
    "s/^    <Version>.+<\/Version>/    <Version>${version}<\/Version>/" \
    Directory.Build.props
  rm -f Directory.Build.props.bak
  git add Directory.Build.props
  cd -

  cd "${SOURCE_DIR}/../../dev/tasks/homebrew-formulae"
  sed -i.bak -E -e \
    "s/arrow-[0-9.]+[0-9]+/arrow-${r_version}/g" \
    autobrew/apache-arrow.rb
  rm -f autobrew/apache-arrow.rb.bak
  git add autobrew/apache-arrow.rb
  sed -i.bak -E -e \
    "s/arrow-[0-9.\-]+[0-9SNAPHOT]+/arrow-${version}/g" \
    apache-arrow.rb
  rm -f apache-arrow.rb.bak
  git add apache-arrow.rb
  cd -

  cd "${SOURCE_DIR}/../../js"
  sed -i.bak -E -e \
    "s/^  \"version\": \".+\"/  \"version\": \"${version}\"/" \
    package.json
  rm -f package.json.bak
  git add package.json
  cd -

  cd "${SOURCE_DIR}/../../matlab"
  sed -i.bak -E -e \
    "s/^set\(MLARROW_VERSION \".+\"\)/set(MLARROW_VERSION \"${version}\")/" \
    CMakeLists.txt
  rm -f CMakeLists.txt.bak
  git add CMakeLists.txt
  cd -

  cd "${SOURCE_DIR}/../../python"
  sed -i.bak -E -e \
    "s/^default_version = '.+'/default_version = '${version}'/" \
    setup.py
  rm -f setup.py.bak
  git add setup.py
  cd -

  cd "${SOURCE_DIR}/../../r"
  sed -i.bak -E -e \
    "s/^Version: .+/Version: ${r_version}/" \
    DESCRIPTION
  rm -f DESCRIPTION.bak
  git add DESCRIPTION
  if [ ${type} = "snapshot" ]; then
    # Add a news entry for the new dev version
    echo "dev"
    sed -i.bak -E -e \
      "0,/^# arrow /s/^(# arrow .+)/# arrow ${r_version}\n\n\1/" \
      NEWS.md
  else
    # Replace dev version with release version
    echo "release"
    sed -i.bak -E -e \
      "0,/^# arrow /s/^# arrow .+/# arrow ${r_version}/" \
      NEWS.md
  fi
  rm -f NEWS.md.bak
  git add NEWS.md
  cd -

  cd "${SOURCE_DIR}/../../ruby"
  sed -i.bak -E -e \
    "s/^  VERSION = \".+\"/  VERSION = \"${version}\"/g" \
    */*/*/version.rb
  rm -f */*/*/version.rb.bak
  git add */*/*/version.rb
  cd -

  cd "${SOURCE_DIR}/../../rust"
  sed -i.bak -E \
    -e "s/^version = \".+\"/version = \"${version}\"/g" \
    -e "s/^(arrow = .* version = )\".+\"( .*)/\\1\"${version}\"\\2/g" \
    -e "s/^(arrow-flight = .* version = )\".+\"( .*)/\\1\"${version}\"\\2/g" \
    -e "s/^(parquet = .* version = )\".+\"( .*)/\\1\"${version}\"\\2/g" \
    */Cargo.toml
  rm -f */Cargo.toml.bak
  git add */Cargo.toml

  # Update version number for parquet README
  sed -i.bak -E -e \
      "s/^parquet = \".+\"/parquet = \"${version}\"/g" \
      parquet/README.md
  sed -i.bak -E -e \
      "s/docs.rs\/crate\/parquet\/.+\)/docs.rs\/crate\/parquet\/${version}\)/g" \
      parquet/README.md
  rm -f parquet/README.md.bak
  git add parquet/README.md

  # Update version number for datafusion README
  sed -i.bak -E -e \
      "s/^datafusion = \".+\"/datafusion = \"${version}\"/g" \
      datafusion/README.md
  sed -i.bak -E -e \
      "s/docs.rs\/crate\/datafusion\/.+\)/docs.rs\/crate\/datafusion\/${version}\)/g" \
      datafusion/README.md
  rm -f datafusion/README.md.bak
  git add datafusion/README.md
  cd -
}

############################## Pre-Tag Commits ##############################

version=$1
next_version=$2
next_version_snapshot=${next_version}-SNAPSHOT
tag=apache-arrow-${version}

: ${PREPARE_DEFAULT:=1}
: ${PREPARE_CHANGELOG:=${PREPARE_DEFAULT}}
: ${PREPARE_LINUX_PACKAGES:=${PREPARE_DEFAULT}}
: ${PREPARE_VERSION_PRE_TAG:=${PREPARE_DEFAULT}}
: ${PREPARE_TAG:=${PREPARE_DEFAULT}}
: ${PREPARE_VERSION_POST_TAG:=${PREPARE_DEFAULT}}
: ${PREPARE_DEB_PACKAGE_NAMES:=${PREPARE_DEFAULT}}

if [ ${PREPARE_CHANGELOG} -gt 0 ]; then
  echo "Updating changelog for $version"
  # Update changelog
  $SOURCE_DIR/update-changelog.sh $version
fi

if [ ${PREPARE_LINUX_PACKAGES} -gt 0 ]; then
  echo "Updating .deb/.rpm changelogs for $version"
  cd $SOURCE_DIR/../tasks/linux-packages
  rake \
    version:update \
    ARROW_RELEASE_TIME="$(date +%Y-%m-%dT%H:%M:%S%z)" \
    ARROW_VERSION=${version}
  git add */debian*/changelog */yum/*.spec.in
  git commit -m "[Release] Update .deb/.rpm changelogs for $version"
  cd -
fi

if [ ${PREPARE_VERSION_PRE_TAG} -gt 0 ]; then
  echo "prepare release ${version} on tag ${tag} then reset to version ${next_version_snapshot}"

  update_versions "${version}" "${next_version}" "release"
  git commit -m "[Release] Update versions for ${version}"
fi

if [ ${PREPARE_TAG} -gt 0 ]; then
  profile=arrow-jni # this includes components which depend on arrow cpp.
  pushd "${SOURCE_DIR}/../../java"
  git submodule update --init --recursive
  cpp_dir="${PWD}/../cpp"
  cpp_build_dir=$(mktemp -d -t "apache-arrow-cpp.XXXXX")
  pushd ${cpp_build_dir}
  cmake \
    -DARROW_GANDIVA=ON \
    -DARROW_GANDIVA_JAVA=ON \
    -DARROW_JNI=ON \
    -DARROW_ORC=ON \
    -DCMAKE_BUILD_TYPE=release \
    -G Ninja \
    "${cpp_dir}"
  ninja
  popd
  mvn release:clean
  mvn \
    release:prepare \
    -Darguments=-Darrow.cpp.build.dir=${cpp_build_dir}/release \
    -DautoVersionSubmodules \
    -DdevelopmentVersion=${next_version_snapshot} \
    -DreleaseVersion=${version} \
    -Dtag=${tag} \
    -P ${profile}
  rm -rf ${cpp_build_dir}
  popd
fi

############################## Post-Tag Commits #############################

if [ ${PREPARE_VERSION_POST_TAG} -gt 0 ]; then
  echo "Updating versions for ${next_version_snapshot}"
  update_versions "${version}" "${next_version}" "snapshot"
  git commit -m "[Release] Update versions for ${next_version_snapshot}"
fi

if [ ${PREPARE_DEB_PACKAGE_NAMES} -gt 0 ]; then
  echo "Updating .deb package names for ${next_version}"
  so_version() {
    local version=$1
    local major_version=$(echo $version | sed -E -e 's/^([0-9]+)\.[0-9]+\.[0-9]+$/\1/')
    local minor_version=$(echo $version | sed -E -e 's/^[0-9]+\.([0-9]+)\.[0-9]+$/\1/')
    expr ${major_version} \* 100 + ${minor_version}
  }
  deb_lib_suffix=$(so_version $version)
  next_deb_lib_suffix=$(so_version $next_version)
  if [ "${deb_lib_suffix}" != "${next_deb_lib_suffix}" ]; then
    cd $SOURCE_DIR/../tasks/linux-packages/apache-arrow
    for target in debian*/lib*${deb_lib_suffix}.install; do
      git mv \
	${target} \
	$(echo $target | sed -e "s/${deb_lib_suffix}/${next_deb_lib_suffix}/")
    done
    deb_lib_suffix_substitute_pattern="s/(lib(arrow|gandiva|parquet|plasma)[-a-z]*)${deb_lib_suffix}/\\1${next_deb_lib_suffix}/g"
    sed -i.bak -E -e "${deb_lib_suffix_substitute_pattern}" debian*/control
    rm -f debian*/control.bak
    git add debian*/control
    cd -
    cd $SOURCE_DIR/../tasks/
    sed -i.bak -E -e "${deb_lib_suffix_substitute_pattern}" tasks.yml
    rm -f tasks.yml.bak
    git add tasks.yml
    cd -
    cd $SOURCE_DIR
    sed -i.bak -E -e "${deb_lib_suffix_substitute_pattern}" rat_exclude_files.txt
    rm -f rat_exclude_files.txt.bak
    git add rat_exclude_files.txt
    git commit -m "[Release] Update .deb package names for $next_version"
    cd -
  fi
fi

echo "Finish staging binary artifacts by running: dev/release/01-perform.sh"
