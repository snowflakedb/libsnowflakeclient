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
include tests/CMakeFiles/test_check_ctypes.dir/depend.make

# Include the progress variables for this target.
include tests/CMakeFiles/test_check_ctypes.dir/progress.make

# Include the compile flags for this target's objects.
include tests/CMakeFiles/test_check_ctypes.dir/flags.make

tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o: tests/CMakeFiles/test_check_ctypes.dir/flags.make
tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o: ../tests/utils/test_setup.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/mnt/host/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o   -c /mnt/host/tests/utils/test_setup.c

tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.i"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /mnt/host/tests/utils/test_setup.c > CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.i

tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.s"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /mnt/host/tests/utils/test_setup.c -o CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.s

tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o.requires:

.PHONY : tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o.requires

tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o.provides: tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o.requires
	$(MAKE) -f tests/CMakeFiles/test_check_ctypes.dir/build.make tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o.provides.build
.PHONY : tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o.provides

tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o.provides.build: tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o


tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o: tests/CMakeFiles/test_check_ctypes.dir/flags.make
tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o: ../tests/utils/mock_setup.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/mnt/host/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o   -c /mnt/host/tests/utils/mock_setup.c

tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.i"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /mnt/host/tests/utils/mock_setup.c > CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.i

tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.s"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /mnt/host/tests/utils/mock_setup.c -o CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.s

tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o.requires:

.PHONY : tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o.requires

tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o.provides: tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o.requires
	$(MAKE) -f tests/CMakeFiles/test_check_ctypes.dir/build.make tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o.provides.build
.PHONY : tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o.provides

tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o.provides.build: tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o


tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o: tests/CMakeFiles/test_check_ctypes.dir/flags.make
tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o: ../tests/test_check_ctypes.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/mnt/host/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building C object tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o   -c /mnt/host/tests/test_check_ctypes.c

tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.i"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /mnt/host/tests/test_check_ctypes.c > CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.i

tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.s"
	cd /mnt/host/cmake-build/tests && /usr/lib64/ccache/gcc52  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /mnt/host/tests/test_check_ctypes.c -o CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.s

tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o.requires:

.PHONY : tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o.requires

tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o.provides: tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o.requires
	$(MAKE) -f tests/CMakeFiles/test_check_ctypes.dir/build.make tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o.provides.build
.PHONY : tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o.provides

tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o.provides.build: tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o


# Object files for target test_check_ctypes
test_check_ctypes_OBJECTS = \
"CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o" \
"CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o" \
"CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o"

# External object files for target test_check_ctypes
test_check_ctypes_EXTERNAL_OBJECTS =

tests/test_check_ctypes: tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o
tests/test_check_ctypes: tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o
tests/test_check_ctypes: tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o
tests/test_check_ctypes: tests/CMakeFiles/test_check_ctypes.dir/build.make
tests/test_check_ctypes: libsnowflakeclient.a
tests/test_check_ctypes: ../deps-build/linux/cmocka/lib/libcmocka.a
tests/test_check_ctypes: ../deps-build/linux/azure/lib/libazure-storage-lite.a
tests/test_check_ctypes: ../deps-build/linux/uuid/lib/libuuid.a
tests/test_check_ctypes: tests/CMakeFiles/test_check_ctypes.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/mnt/host/cmake-build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking CXX executable test_check_ctypes"
	cd /mnt/host/cmake-build/tests && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/test_check_ctypes.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
tests/CMakeFiles/test_check_ctypes.dir/build: tests/test_check_ctypes

.PHONY : tests/CMakeFiles/test_check_ctypes.dir/build

tests/CMakeFiles/test_check_ctypes.dir/requires: tests/CMakeFiles/test_check_ctypes.dir/utils/test_setup.c.o.requires
tests/CMakeFiles/test_check_ctypes.dir/requires: tests/CMakeFiles/test_check_ctypes.dir/utils/mock_setup.c.o.requires
tests/CMakeFiles/test_check_ctypes.dir/requires: tests/CMakeFiles/test_check_ctypes.dir/test_check_ctypes.c.o.requires

.PHONY : tests/CMakeFiles/test_check_ctypes.dir/requires

tests/CMakeFiles/test_check_ctypes.dir/clean:
	cd /mnt/host/cmake-build/tests && $(CMAKE_COMMAND) -P CMakeFiles/test_check_ctypes.dir/cmake_clean.cmake
.PHONY : tests/CMakeFiles/test_check_ctypes.dir/clean

tests/CMakeFiles/test_check_ctypes.dir/depend:
	cd /mnt/host/cmake-build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /mnt/host /mnt/host/tests /mnt/host/cmake-build /mnt/host/cmake-build/tests /mnt/host/cmake-build/tests/CMakeFiles/test_check_ctypes.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : tests/CMakeFiles/test_check_ctypes.dir/depend
