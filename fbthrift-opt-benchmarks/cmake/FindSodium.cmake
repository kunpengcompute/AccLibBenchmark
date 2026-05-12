find_path(Sodium_INCLUDE_DIR
  NAMES sodium.h
  PATHS
    /usr/local/include
    /usr/include
)

find_library(Sodium_LIBRARY
  NAMES sodium libsodium
  PATHS
    /usr/local/lib
    /usr/lib
    /usr/lib/aarch64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sodium
  REQUIRED_VARS Sodium_INCLUDE_DIR Sodium_LIBRARY
)

if(Sodium_FOUND AND NOT TARGET Sodium::sodium)
  add_library(Sodium::sodium UNKNOWN IMPORTED)
  set_target_properties(Sodium::sodium PROPERTIES
    IMPORTED_LOCATION "${Sodium_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Sodium_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(Sodium_INCLUDE_DIR Sodium_LIBRARY)
