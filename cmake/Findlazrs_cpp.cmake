# Find LAZRS_CPP
#
# This sets:
#   - LAZRS_CPP_FOUND:  system has LAZRS_CPP
#   - LAZRS_CPP_INCLUDE_DIRS: the LAZRS_CPP include directories
#   - LAZRS_CPP_LIBRARIES: the LAZRS_CPP library
#   - LAZRS_CPP_VERSION: the version string for LAZRS_CPP

find_path (LAZRS_CPP_INCLUDE_DIRS NAMES lazrs/lazrs.h)
find_library (LAZRS_CPP_LIBRARY NAMES lazrs_cpp)

set (LAZRS_CPP_LIBRARIES ${LAZRS_CPP_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(lazrs_cpp
    REQUIRED_VARS LAZRS_CPP_INCLUDE_DIRS LAZRS_CPP_LIBRARY
    VERSION_VAR LAZRS_CPP_VERSION_STRING
)

mark_as_advanced(LAZRS_CPP_LIBRARY LAZRS_CPP_LIBRARIES LAZRS_CPP_INCLUDE_DIRS)
