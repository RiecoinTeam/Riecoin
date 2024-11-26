# Copyright (c) 2024-present The Riecoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

find_path(GMP_INCLUDE_DIR
  NAMES gmp.h
  NAMES gmpxx.h
)

find_library(GMP_LIBRARY
  NAMES gmp
  NAMES gmpxx
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP
  REQUIRED_VARS GMP_LIBRARY GMP_INCLUDE_DIR
)

if(GMP_FOUND AND NOT TARGET GMP::GMP)
  add_library(GMP::GMP UNKNOWN IMPORTED)
  set_target_properties(GMP::GMP PROPERTIES
    IMPORTED_LOCATION "${GMP_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${GMP_INCLUDE_DIR}"
  )
  set_property(TARGET GMP::GMP PROPERTY
    INTERFACE_COMPILE_DEFINITIONS USE_GMP=1 $<$<PLATFORM_ID:Windows>:GMP_STATICLIB>
  )
endif()

mark_as_advanced(
  GMP_INCLUDE_DIR
  GMP_LIBRARY
)
