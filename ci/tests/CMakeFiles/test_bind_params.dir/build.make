# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.6

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Produce verbose output by default.
VERBOSE = 1

# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake3

# The command to remove a file.
RM = /usr/bin/cmake3 -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /mnt/host

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /mnt/host/cmake-build

# Include any dependencies generated for this target.
include tests/CMakeFiles/test_bind_params.dir/depend.make

# Include the progress variables for this target.
include tests/CMakeFiles/test_bind_params.dir/progress.make

# Include the compile flags for this target's objects.
include tests/CMakeFiles/test_bind_params.dir/flags.make

tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o: tests/CMakeFiles/test_bind_params.dir/flags.make
tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o: ../tests/utils/test_setup.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/mnt/host/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/test_bind_params.dir/utils/test_setup.c.o   -c /mnt/host/tests/utils/test_setup.c

tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_bind_params.dir/utils/test_setup.c.i"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /mnt/host/tests/utils/test_setup.c > CMakeFiles/test_bind_params.dir/utils/test_setup.c.i

tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_bind_params.dir/utils/test_setup.c.s"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /mnt/host/tests/utils/test_setup.c -o CMakeFiles/test_bind_params.dir/utils/test_setup.c.s

tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o.requires:

.PHONY : tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o.requires

tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o.provides: tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o.requires
	$(MAKE) -f tests/CMakeFiles/test_bind_params.dir/build.make tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o.provides.build
.PHONY : tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o.provides

tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o.provides.build: tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o


tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o: tests/CMakeFiles/test_bind_params.dir/flags.make
tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o: ../tests/utils/mock_setup.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/mnt/host/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o   -c /mnt/host/tests/utils/mock_setup.c

tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_bind_params.dir/utils/mock_setup.c.i"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /mnt/host/tests/utils/mock_setup.c > CMakeFiles/test_bind_params.dir/utils/mock_setup.c.i

tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_bind_params.dir/utils/mock_setup.c.s"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /mnt/host/tests/utils/mock_setup.c -o CMakeFiles/test_bind_params.dir/utils/mock_setup.c.s

tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o.requires:

.PHONY : tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o.requires

tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o.provides: tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o.requires
	$(MAKE) -f tests/CMakeFiles/test_bind_params.dir/build.make tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o.provides.build
.PHONY : tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o.provides

tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o.provides.build: tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o


tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o: tests/CMakeFiles/test_bind_params.dir/flags.make
tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o: ../tests/test_bind_params.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/mnt/host/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building C object tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/test_bind_params.dir/test_bind_params.c.o   -c /mnt/host/tests/test_bind_params.c

tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_bind_params.dir/test_bind_params.c.i"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /mnt/host/tests/test_bind_params.c > CMakeFiles/test_bind_params.dir/test_bind_params.c.i

tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_bind_params.dir/test_bind_params.c.s"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /mnt/host/tests/test_bind_params.c -o CMakeFiles/test_bind_params.dir/test_bind_params.c.s

tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o.requires:

.PHONY : tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o.requires

tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o.provides: tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o.requires
	$(MAKE) -f tests/CMakeFiles/test_bind_params.dir/build.make tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o.provides.build
.PHONY : tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o.provides

tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o.provides.build: tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o


# Object files for target test_bind_params
test_bind_params_OBJECTS = \
"CMakeFiles/test_bind_params.dir/utils/test_setup.c.o" \
"CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o" \
"CMakeFiles/test_bind_params.dir/test_bind_params.c.o"

# External object files for target test_bind_params
test_bind_params_EXTERNAL_OBJECTS =

tests/test_bind_params: tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o
tests/test_bind_params: tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o
tests/test_bind_params: tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o
tests/test_bind_params: tests/CMakeFiles/test_bind_params.dir/build.make
tests/test_bind_params: libsnowflakeclient.a
tests/test_bind_params: ../deps-build/linux/cmocka/lib/libcmocka.a
tests/test_bind_params: ../deps-build/linux/azure/lib/libazure-storage-lite.a
tests/test_bind_params: ../deps-build/linux/uuid/lib/libuuid.a
tests/test_bind_params: tests/CMakeFiles/test_bind_params.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/mnt/host/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking CXX executable test_bind_params"
	cd /mnt/host/cmake-build/tests && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/test_bind_params.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
tests/CMakeFiles/test_bind_params.dir/build: tests/test_bind_params

.PHONY : tests/CMakeFiles/test_bind_params.dir/build

tests/CMakeFiles/test_bind_params.dir/requires: tests/CMakeFiles/test_bind_params.dir/utils/test_setup.c.o.requires
tests/CMakeFiles/test_bind_params.dir/requires: tests/CMakeFiles/test_bind_params.dir/utils/mock_setup.c.o.requires
tests/CMakeFiles/test_bind_params.dir/requires: tests/CMakeFiles/test_bind_params.dir/test_bind_params.c.o.requires

.PHONY : tests/CMakeFiles/test_bind_params.dir/requires

tests/CMakeFiles/test_bind_params.dir/clean:
	cd /mnt/host/cmake-build/tests && $(CMAKE_COMMAND) -P CMakeFiles/test_bind_params.dir/cmake_clean.cmake
.PHONY : tests/CMakeFiles/test_bind_params.dir/clean

tests/CMakeFiles/test_bind_params.dir/depend:
	cd /mnt/host/cmake-build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /mnt/host /mnt/host/tests /mnt/host/cmake-build /mnt/host/cmake-build/tests /mnt/host/cmake-build/tests/CMakeFiles/test_bind_params.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : tests/CMakeFiles/test_bind_params.dir/depend
