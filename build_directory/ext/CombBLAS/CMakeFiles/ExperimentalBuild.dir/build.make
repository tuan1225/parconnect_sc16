# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

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

# Utility rule file for ExperimentalBuild.

# Include the progress variables for this target.
include ext/CombBLAS/CMakeFiles/ExperimentalBuild.dir/progress.make

ext/CombBLAS/CMakeFiles/ExperimentalBuild:
	cd /home/tuan/parconnect_SCC16/build_directory/ext/CombBLAS && /usr/bin/ctest -D ExperimentalBuild

ExperimentalBuild: ext/CombBLAS/CMakeFiles/ExperimentalBuild
ExperimentalBuild: ext/CombBLAS/CMakeFiles/ExperimentalBuild.dir/build.make
.PHONY : ExperimentalBuild

# Rule to build all files generated by this target.
ext/CombBLAS/CMakeFiles/ExperimentalBuild.dir/build: ExperimentalBuild
.PHONY : ext/CombBLAS/CMakeFiles/ExperimentalBuild.dir/build

ext/CombBLAS/CMakeFiles/ExperimentalBuild.dir/clean:
	cd /home/tuan/parconnect_SCC16/build_directory/ext/CombBLAS && $(CMAKE_COMMAND) -P CMakeFiles/ExperimentalBuild.dir/cmake_clean.cmake
.PHONY : ext/CombBLAS/CMakeFiles/ExperimentalBuild.dir/clean

ext/CombBLAS/CMakeFiles/ExperimentalBuild.dir/depend:
	cd /home/tuan/parconnect_SCC16/build_directory && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/tuan/parconnect_SCC16 /home/tuan/parconnect_SCC16/ext/CombBLAS /home/tuan/parconnect_SCC16/build_directory /home/tuan/parconnect_SCC16/build_directory/ext/CombBLAS /home/tuan/parconnect_SCC16/build_directory/ext/CombBLAS/CMakeFiles/ExperimentalBuild.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : ext/CombBLAS/CMakeFiles/ExperimentalBuild.dir/depend

