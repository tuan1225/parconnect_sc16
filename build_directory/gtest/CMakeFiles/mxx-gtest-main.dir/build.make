# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

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
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = /usr/bin/ccmake

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/tuan/parconnect_SCC16

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/tuan/parconnect_SCC16/build_directory

# Include any dependencies generated for this target.
include gtest/CMakeFiles/mxx-gtest-main.dir/depend.make

# Include the progress variables for this target.
include gtest/CMakeFiles/mxx-gtest-main.dir/progress.make

# Include the compile flags for this target's objects.
include gtest/CMakeFiles/mxx-gtest-main.dir/flags.make

gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o: gtest/CMakeFiles/mxx-gtest-main.dir/flags.make
gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o: ../gtest/mxx_gtest_main.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/tuan/parconnect_SCC16/build_directory/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o"
	cd /home/tuan/parconnect_SCC16/build_directory/gtest && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o -c /home/tuan/parconnect_SCC16/gtest/mxx_gtest_main.cpp

gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.i"
	cd /home/tuan/parconnect_SCC16/build_directory/gtest && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/tuan/parconnect_SCC16/gtest/mxx_gtest_main.cpp > CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.i

gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.s"
	cd /home/tuan/parconnect_SCC16/build_directory/gtest && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/tuan/parconnect_SCC16/gtest/mxx_gtest_main.cpp -o CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.s

gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o.requires:
.PHONY : gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o.requires

gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o.provides: gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o.requires
	$(MAKE) -f gtest/CMakeFiles/mxx-gtest-main.dir/build.make gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o.provides.build
.PHONY : gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o.provides

gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o.provides.build: gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o

gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o: gtest/CMakeFiles/mxx-gtest-main.dir/flags.make
gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o: ../gtest/gtest-all.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/tuan/parconnect_SCC16/build_directory/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o"
	cd /home/tuan/parconnect_SCC16/build_directory/gtest && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o -c /home/tuan/parconnect_SCC16/gtest/gtest-all.cc

gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.i"
	cd /home/tuan/parconnect_SCC16/build_directory/gtest && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/tuan/parconnect_SCC16/gtest/gtest-all.cc > CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.i

gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.s"
	cd /home/tuan/parconnect_SCC16/build_directory/gtest && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/tuan/parconnect_SCC16/gtest/gtest-all.cc -o CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.s

gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o.requires:
.PHONY : gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o.requires

gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o.provides: gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o.requires
	$(MAKE) -f gtest/CMakeFiles/mxx-gtest-main.dir/build.make gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o.provides.build
.PHONY : gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o.provides

gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o.provides.build: gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o

# Object files for target mxx-gtest-main
mxx__gtest__main_OBJECTS = \
"CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o" \
"CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o"

# External object files for target mxx-gtest-main
mxx__gtest__main_EXTERNAL_OBJECTS =

lib/libmxx-gtest-main.so: gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o
lib/libmxx-gtest-main.so: gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o
lib/libmxx-gtest-main.so: gtest/CMakeFiles/mxx-gtest-main.dir/build.make
lib/libmxx-gtest-main.so: /usr/lib64/openmpi/lib/libmpi_cxx.so
lib/libmxx-gtest-main.so: /usr/lib64/openmpi/lib/libmpi.so
lib/libmxx-gtest-main.so: gtest/CMakeFiles/mxx-gtest-main.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX shared library ../lib/libmxx-gtest-main.so"
	cd /home/tuan/parconnect_SCC16/build_directory/gtest && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/mxx-gtest-main.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
gtest/CMakeFiles/mxx-gtest-main.dir/build: lib/libmxx-gtest-main.so
.PHONY : gtest/CMakeFiles/mxx-gtest-main.dir/build

gtest/CMakeFiles/mxx-gtest-main.dir/requires: gtest/CMakeFiles/mxx-gtest-main.dir/mxx_gtest_main.cpp.o.requires
gtest/CMakeFiles/mxx-gtest-main.dir/requires: gtest/CMakeFiles/mxx-gtest-main.dir/gtest-all.cc.o.requires
.PHONY : gtest/CMakeFiles/mxx-gtest-main.dir/requires

gtest/CMakeFiles/mxx-gtest-main.dir/clean:
	cd /home/tuan/parconnect_SCC16/build_directory/gtest && $(CMAKE_COMMAND) -P CMakeFiles/mxx-gtest-main.dir/cmake_clean.cmake
.PHONY : gtest/CMakeFiles/mxx-gtest-main.dir/clean

gtest/CMakeFiles/mxx-gtest-main.dir/depend:
	cd /home/tuan/parconnect_SCC16/build_directory && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/tuan/parconnect_SCC16 /home/tuan/parconnect_SCC16/gtest /home/tuan/parconnect_SCC16/build_directory /home/tuan/parconnect_SCC16/build_directory/gtest /home/tuan/parconnect_SCC16/build_directory/gtest/CMakeFiles/mxx-gtest-main.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : gtest/CMakeFiles/mxx-gtest-main.dir/depend

