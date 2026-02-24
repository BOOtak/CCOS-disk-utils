set(CCOS_FAKETIME "1985-09-26 13:31:01.800" CACHE STRING "Fake wall-clock time injected into tests via libfaketime (FAKETIME env var)")
set(CCOS_FAKETIME_LIB "" CACHE FILEPATH "Absolute path to libfaketime library used for preloading (overrides auto-detection)")

if(APPLE)
  # macOS uses DYLD_* interposing instead of LD_PRELOAD.
  # We must use an absolute path; a bare 'libfaketime.dylib' typically won't be found by dyld.
  # If auto-detection fails, set it explicitly: -DCCOS_FAKETIME_LIB=/path/to/libfaketime.1.dylib
  if(CCOS_FAKETIME_LIB STREQUAL "")
    find_library(_ccos_faketime_lib
      # find_library() adds 'lib' prefix and platform suffixes automatically,
      # so use names without the 'lib' prefix.
      NAMES faketime.1 faketime
      PATHS
        /opt/homebrew/opt/libfaketime/lib/faketime
        /opt/homebrew/lib/faketime
        /opt/homebrew/lib
        /usr/local/lib/faketime
        /usr/local/lib
    )
    if(NOT _ccos_faketime_lib)
      message(FATAL_ERROR "libfaketime not found. Set -DCCOS_FAKETIME_LIB=/absolute/path/to/libfaketime.1.dylib")
    endif()
    set(CCOS_FAKETIME_LIB "${_ccos_faketime_lib}" CACHE FILEPATH "Absolute path to libfaketime library used for preloading (overrides auto-detection)" FORCE)
  endif()
  set_tests_properties(ccos_tests PROPERTIES ENVIRONMENT
    "FAKETIME=${CCOS_FAKETIME};DYLD_INSERT_LIBRARIES=${CCOS_FAKETIME_LIB};DYLD_FORCE_FLAT_NAMESPACE=1"
  )
elseif(UNIX)
  # Linux/BSD typically use LD_PRELOAD.
  # If auto-detection fails, set it explicitly: -DCCOS_FAKETIME_LIB=/path/to/libfaketime.so.1
  if(CCOS_FAKETIME_LIB STREQUAL "")
    find_library(_ccos_faketime_lib
      NAMES faketime.so.1 faketime
    )
    if(NOT _ccos_faketime_lib)
      message(FATAL_ERROR "libfaketime not found. Set -DCCOS_FAKETIME_LIB=/absolute/path/to/libfaketime.so.1")
    endif()
    set(CCOS_FAKETIME_LIB "${_ccos_faketime_lib}" CACHE FILEPATH "Absolute path to libfaketime library used for preloading (overrides auto-detection)" FORCE)
  endif()
  set_tests_properties(ccos_tests PROPERTIES ENVIRONMENT
    "FAKETIME=${CCOS_FAKETIME};LD_PRELOAD=${CCOS_FAKETIME_LIB}"
  )
endif()

