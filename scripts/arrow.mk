# We prefix commands with a backslash to avoid alias expansion.
RM = \rm -rf
RM_F = \rm -f
CP = \cp -r
CD = \cd
MKDIR_P = mkdir -p
WGET = wget

# Define our base cmake command
CMAKE_3 = cmake3 -DCMAKE_C_COMPILER=${CC} -DCMAKE_CXX_COMPILER=${CXX} -DCMAKE_LINKER=${LD} -DCMAKE_BUILD_TYPE=${BUILD_TYPE}

SHELL := /bin/bash

BUILD_DIR := arrow/cpp/build
ARROW_DEPS_INSTALL_DIR := ../deps-build/linux/${BUILD_TYPE}/arrow_deps
ARROW_INSTALL_DIR := ../deps-build/linux/${BUILD_TYPE}/arrow
BOOST_INSTALL_DIR := ../deps-build/linux/${BUILD_TYPE}/boost

all: build copy

copy:
	cp ${BUILD_DIR}/boost_ep-prefix/src/boost_ep/stage/lib/*.a $(BOOST_INSTALL_DIR)/lib || true
	cp ${BUILD_DIR}/brotli_ep/src/brotli_ep-install/lib/*.a $(ARROW_DEPS_INSTALL_DIR)/lib || true
	cp ${BUILD_DIR}/double-conversion_ep/src/double-conversion_ep/lib/libdouble-conversion.a $(ARROW_DEPS_INSTALL_DIR)/lib || true
	cp ${BUILD_DIR}/flatbuffers_ep-prefix/src/flatbuffers_ep-install/lib64/libflatbuffers.a $(ARROW_DEPS_INSTALL_DIR)/lib || true
	cp ${BUILD_DIR}/glog_ep-prefix/src/glog_ep/lib/libglog.a $(ARROW_DEPS_INSTALL_DIR)/lib || true
	cp ${BUILD_DIR}/jemalloc_ep-prefix/src/jemalloc_ep/lib/libjemalloc.a $(ARROW_DEPS_INSTALL_DIR)/lib || true
	cp ${BUILD_DIR}/jemalloc_ep-prefix/src/jemalloc_ep/lib/libjemalloc_pic.a $(ARROW_DEPS_INSTALL_DIR)/lib || true
	cp ${BUILD_DIR}/jemalloc_ep-prefix/src/jemalloc_ep/include/jemalloc/jemalloc.h $(ARROW_DEPS_INSTALL_DIR)/include/ || true
	cp ${BUILD_DIR}/lz4_ep-prefix/src/lz4_ep/lib/liblz4.a $(ARROW_DEPS_INSTALL_DIR)/lib || true
	cp ${BUILD_DIR}/snappy_ep/src/snappy_ep-install/lib/libsnappy.a $(ARROW_DEPS_INSTALL_DIR)/lib || true
	cp ${BUILD_DIR}/zstd_ep-install/lib64/libzstd.a $(ARROW_DEPS_INSTALL_DIR)/lib || true
	cd $(ARROW_INSTALL_DIR)/../ && tar czf arrow_linux_$(BUILD_TYPE)-$(ARROW_VER).tar.gz arrow arrow_deps boost

# arrow build target
build: create_installdir arrow_configure
	# The current arrow release has an issue that prevents it from being built
	# with ccache. Explicitly disable ccache during the build to avoid this.
	$(MAKE) -C ${BUILD_DIR} install

create_installdir:
	${MKDIR_P} ${BUILD_DIR}
	${MKDIR_P} ${ARROW_DEPS_INSTALL_DIR}/lib
	${MKDIR_P} ${ARROW_DEPS_INSTALL_DIR}/include
	${MKDIR_P} ${BOOST_INSTALL_DIR}/lib
	${MKDIR_P} ${BOOST_INSTALL_DIR}/include
	${MKDIR_P} ${ARROW_INSTALL_DIR}/lib64
	${MKDIR_P} ${ARROW_INSTALL_DIR}/include

arrow_configure:
	cd ${BUILD_DIR}/ && \
	$(CMAKE_3) \
		-DARROW_BUILD_TESTS=OFF \
		-DARROW_IPC=ON \
		-DBOOST_ROOT=../../../$(BOOST_INSTALL_DIR) \
		-DPYTHON_EXECUTABLE:FILEPATH=`which python3.6` \
		-DARROW_CXXFLAGS="-march=ivybridge -std=c++11 -fPIC " \
		-DARROW_BUILD_STATIC=ON \
		-DARROW_BUILD_SHARED=OFF \
		-DCMAKE_INSTALL_PREFIX=../../../$(ARROW_INSTALL_DIR) ../

clean:
	${RM} ${BUILD_DIR} installed apache-arrow* ${ARROW_DEPS_INSTALL_DIR} ${ARROW_INSTALL_DIR} ${BOOST_INSTALL_DIR}

# No need to install anything
install:

.PHONY: all clean install
