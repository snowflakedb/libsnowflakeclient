# We prefix commands with a backslash to avoid alias expansion.
RM = \rm -rf
RM_F = \rm -f
CP = \cp -r
CD = \cd
MKDIR_P = mkdir -p
WGET = wget

# Define our base cmake command
CMAKE_3 = cmake3 -DCMAKE_C_COMPILER=${CC} -DCMAKE_CXX_COMPILER=${CXX} -DCMAKE_LINKER=${LD} -DCMAKE_BUILD_TYPE=${BUILD_TYPE}

# To update arrow, update SRC_URL to point to the new release tarball and SRC_SIG to
# the SHA-256 signature (sha256sum) of the new release tarball.
# Arrow source code: https://github.com/snowflakedb/arrow/tree/apache-arrow-sf-0.15.1
# How to create tarball file?
#   git clone https://github.com/snowflakedb/arrow.git
#   cd arrow
#   git checkout --track origin/apache-arrow-sf-0.15.1
#   cd ..
#   tar -czvf apache-arrow-sf-0.15.1.tar.gz arrow/
#   aws s3 cp apache-arrow-sf-0.15.1.tar.gz s3://sfc-jenkins/xp-dependencies/arrow/

ARROW_VER := 0.15.0
SRC_URL := s3://sfc-jenkins/xp-dependencies/arrow/apache-arrow-sf-0.15.1.tar.gz
SRC_SIG := 8878bf8f728d52f501ad48e311fabbe0fff3ce7f472865890a5a05e6343553c1

SHELL := /bin/bash

ARROW_DEPS_INSTALL_DIR := ../deps-build/linux/${BUILD_TYPE}/arrow_deps
ARROW_INSTALL_DIR := ../deps-build/linux/${BUILD_TYPE}/arrow
BOOST_INSTALL_DIR := ../deps-build/linux/${BUILD_TYPE}/boost

all: build copy

copy:
	cd build && make install
	cd ../
	find build/ -type f \( -iname "libdouble-conversion.a" -o -iname "libflatbuffers.a" -o -iname "libglog.a" -o -iname "libjemalloc.a" -o -iname "libjemalloc_pic.a" -o -iname "liblz4.a" \) | xargs -I{} cp {} $(ARROW_DEPS_INSTALL_DIR)/lib/
	find build/ -type f \( -iname "libboost_filesystem.a" -o -iname "libboost_regex.a" -o -iname "libboost_system.a" \) | xargs -I{} cp {} $(BOOST_INSTALL_DIR)/lib
	cd $(ARROW_INSTALL_DIR)/../ && tar czf arrow_linux_$(BUILD_TYPE)-$(ARROW_VER).tar.gz arrow arrow_deps boost

# arrow build target
build: create_installdir arrow_configure
	# The current arrow release has an issue that prevents it from being built
	# with ccache. Explicitly disable ccache during the build to avoid this.
	$(MAKE) -C build install

create_installdir:
	${MKDIR_P} build
	${MKDIR_P} ${ARROW_DEPS_INSTALL_DIR}/lib
	${MKDIR_P} ${ARROW_DEPS_INSTALL_DIR}/include
	${MKDIR_P} ${BOOST_INSTALL_DIR}/lib
	${MKDIR_P} ${BOOST_INSTALL_DIR}/include
	${MKDIR_P} ${ARROW_INSTALL_DIR}/lib64
	${MKDIR_P} ${ARROW_INSTALL_DIR}/include

arrow_configure: download-src
	cd build/ && \
	$(CMAKE_3) \
		-DARROW_BUILD_TESTS=OFF \
		-DARROW_IPC=ON \
		-DBOOST_ROOT=../$(BOOST_INSTALL_DIR) \
		-DPYTHON_EXECUTABLE:FILEPATH=`which python3.6` \
        -DARROW_CXXFLAGS="-march=ivybridge -std=c++11 -fPIC " \
		-DARROW_BUILD_STATIC=ON \
		-DARROW_BUILD_SHARED=OFF \
        -DCMAKE_INSTALL_PREFIX=../$(ARROW_INSTALL_DIR) ../src/cpp

# Check if the src directory exists. If not, we have to download the source.
SRC_EXISTS := $(shell if [ -d src ]; then echo 1; fi)
ifneq ($(SRC_EXISTS), 1)
	DOWNLOAD_SRC := 1
else
	# Also check if the src directory is the latest version. If not, re-download.
	SRC_VERSION := $(shell cat downloaded_src_url)
	ifneq ($(SRC_VERSION),$(strip $(SRC_URL)))
		DOWNLOAD_SRC := 1
	endif
endif
# Also extract the name of the file we will download to simplify our download target.
SRC_FILE := $(shell basename $(SRC_URL))

# Target to download the src, This target will only be activated if any of
# the above checks enabled DOWNLOAD_SRC. The target will then download the
# tarball from the provided URL, verify that its signature matches the
# provided signature and then unpack the tarball into src.
download-src:
	@echo $(DOWNLOAD_SRC) $(SRC_EXISTS)
ifeq ($(DOWNLOAD_SRC),1)
	@# Now download and unpack the source code into 'src'. We remove the directory
	@# first to ensure we can safely update existing source code.
	@$(RM) src && aws s3 cp --region=us-west-2 $(SRC_URL) $(SRC_FILE)
	@# Next, we validate the md5sum of the tarball to ensure it is the correct archive.
	@if [ `sha256sum $(SRC_FILE) | cut -d ' ' -f 1` != $(SRC_SIG) ]; then echo "Could not verify download integrity (checksum mismatch)" && false; else true; fi
	@# Finally, extract the tarball into the src directory.
	@tar xf $(SRC_FILE) && $(RM) $(SRC_FILE) && mv snowflake-arrow src
	@echo $(SRC_URL) > downloaded_src_url
endif

clean:
	${RM} build installed src downloaded_src_url apache-arrow* ${ARROW_DEPS_INSTALL_DIR} ${ARROW_INSTALL_DIR} ${BOOST_INSTALL_DIR}

# No need to install anything
install:

.PHONY: all clean install
