# created by Martin Rupp
# martin.rupp@gcsc.uni-frankfurt.de

# toolchain file for Hermit/XE6 HLRS Stuttgart


# USAGE
# first change programming environment for your compiler of choice
# for Cray compiler:
#    module swap $(module li 2>&1 | awk '/PrgEnv/{print $2}') PrgEnv-cray
# for GCC:
#    module swap $(module li 2>&1 | awk '/PrgEnv/{print $2}') PrgEnv-gnu
# then start cmake
#    cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain_file_hermit.cmake ..


# on the cray compiler:
# https://fs.hlrs.de/projects/craydoc/docs_merged/books/S-2179-74/html-S-2179-74/lymwlrwh.html#z862002021malz

# check modules:
# module list
# also: module avail / module load / module swap.

# for use on the CRAY XE6 (Hermit) of HLRS/Karlsruhe
# http://www.hlrs.de/systems/platforms/cray-xe6-hermit/

# see also $CRAY_* environment variables
# $CRAY_UGNI_POST_LINK_OPTS

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


SET(CMAKE_Fortran_COMPILER ftn)

SET(CMAKE_C_COMPILER gcc CACHE FORCE "")
SET(CMAKE_CXX_COMPILER g++ CACHE FORCE "")

SET(STATIC ON CACHE FORCE "")