# Find MINILAS
#
# This sets:
#   - MINILAS_FOUND:  system has MINILAS
#   - MINILAS_INCLUDE_DIRS: the MINILAS include directories
#   - MINILAS_LIBRARIES: the MINILAS library
#   - MINILAS_VERSION: the version string for MINILAS

find_path (MINILAS_INCLUDE_DIRS NAMES minilas/las.h)
find_library (MINILAS_LIBRARY NAMES minilas)

set (MINILAS_LIBRARIES ${MINILAS_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(minilas
    REQUIRED_VARS MINILAS_INCLUDE_DIRS MINILAS_LIBRARY
    VERSION_VAR MINILAS_VERSION_STRING
)

mark_as_advanced(MINILAS_LIBRARY MINILAS_LIBRARIES MINILAS_INCLUDE_DIRS)
