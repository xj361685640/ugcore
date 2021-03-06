# Copyright (c) 2012-2014:  G-CSC, Goethe University Frankfurt
# Author: Martin Rupp
# 
# This file is part of UG4.
# 
# UG4 is free software: you can redistribute it and/or modify it under the
# terms of the GNU Lesser General Public License version 3 (as published by the
# Free Software Foundation) with the following additional attribution
# requirements (according to LGPL/GPL v3 §7):
# 
# (1) The following notice must be displayed in the Appropriate Legal Notices
# of covered and combined works: "Based on UG4 (www.ug4.org/license)".
# 
# (2) The following notice must be displayed at a prominent place in the
# terminal output of covered works: "Based on UG4 (www.ug4.org/license)".
# 
# (3) The following bibliography is recommended for citation and must be
# preserved in all covered files:
# "Reiter, S., Vogel, A., Heppner, I., Rupp, M., and Wittum, G. A massively
#   parallel geometric multigrid solver on hierarchically distributed grids.
#   Computing and visualization in science 16, 4 (2013), 151-164"
# "Vogel, A., Reiter, S., Rupp, M., Nägel, A., and Wittum, G. UG4 -- a novel
#   flexible software system for simulating pde based models on high performance
#   computers. Computing and visualization in science 16, 4 (2013), 165-179"
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.


# toolchain file for Hermit/XE6 HLRS Stuttgart


# USAGE
# first change programming environment for your compiler of choice
# for Cray compiler:
#    module swap $(module li 2>&1 | awk '/PrgEnv/{print $2}') PrgEnv-cray
# for GCC:
#    module swap $(module li 2>&1 | awk '/PrgEnv/{print $2}') PrgEnv-gnu
# then start cmake
#    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain/hermit.cmake ..


# on the cray compiler:
# https://fs.hlrs.de/projects/craydoc/docs_merged/books/S-2179-74/html-S-2179-74/lymwlrwh.html#z862002021malz

# check modules:
# module list
# also: module avail / module load / module swap.

# for use on the CRAY XE6 (Hermit) of HLRS/Karlsruhe
# http://www.hlrs.de/systems/platforms/cray-xe6-hermit/

# see also $CRAY_* environment variables
# $CRAY_UGNI_POST_LINK_OPTS

# to use cmake 2.8.7:
# module load tools/cmake/2.8.7

SET(STATIC_BUILD ON CACHE FORCE "")

SET(HERMIT_CMAKE OFF)
IF(CMAKE_VERSION)
 SET(HERMIT_CMAKE ON)
 IF(${CMAKE_VERSION} STRLESS "2.8.7")
  SET(HERMIT_CMAKE OFF)
  ENDIF()
ENDIF()

IF(HERMIT_CMAKE)
    # cmake >= 2.8.7 has Hermit.cmake
    SET(CMAKE_SYSTEM_NAME Hermit)
ELSE()
    # emulating Hermit.cmake
    # this is needed to get rid of -rdynamic flag...
    SET(CMAKE_SYSTEM_NAME Catamount)
    
    SET(MPI_Fortran_NO_INTERROGATE CMAKE_Fortran_COMPILER)
    SET(MPI_LIBRARY -L$(MPICH_DIR)/lib)
    SET(MPI_EXTRA_LIBRARY -L$(MPICH_DIR)/lib)
    SET(BLA_STATIC ON)
    SET(BLA_VENDOR All)
    SET(BLAS_FIND_QUIETLY ON)
    SET(LAPACK_FIND_QUIETLY ON)
    SET(LAPACK_LIBRARIES "/opt/xt-libsci/11.0.05/cray/74/interlagos/lib/libsci_cray.a")
    
    # Cray is not recognized by Cmake < 2.8.7
    EXECUTE_PROCESS(
	        COMMAND "CC" "-V"
	        OUTPUT_VARIABLE cxx_compiler_string
	        ERROR_VARIABLE cxx_compiler_string
     )
	IF(${cxx_compiler_string} MATCHES ".*\nCray.*")
		SET(CMAKE_CXX_COMPILER_ID "Cray")
		SET(CMAKE_CXX_COMPILER_ID_RUN 1)
	ENDIF()
	
	EXECUTE_PROCESS(
	        COMMAND "cc" "-V"
	        OUTPUT_VARIABLE c_compiler_string
	        ERROR_VARIABLE c_compiler_string
     )
	IF(${c_compiler_string} MATCHES ".*\nCray.*")
		SET(CMAKE_C_COMPILER_ID "Cray")
		SET(CMAKE_C_COMPILER_ID_RUN 1)
	ENDIF()
    
ENDIF()


SET(CMAKE_Fortran_COMPILER ftn)

SET(CMAKE_C_COMPILER cc CACHE FORCE "")
SET(CMAKE_CXX_COMPILER CC CACHE FORCE "")

# be sure that module xt-libsci is loaded
SET(BUILTIN_LAPACK YES CACHE FORCE "")
SET(BUILTIN_BLAS YES CACHE FORCE "")
SET(BUILTIN_MPI YES CACHE FORCE "")

# add the CLOCK_FIX, see ugbase/ug_shell/clock_fix.cpp
SET(CLOCK_FIX ON CACHE FORCE "")

