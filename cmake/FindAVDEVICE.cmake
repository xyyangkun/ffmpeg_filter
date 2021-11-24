find_path(AVDEVICE_INCLUDE_DIR
  NAMES libavdevice/avdevice.h)

find_library(AVDEVICE_LIBRARY
  NAMES avdevice)

set(AVDEVICE_LIBRARIES ${AVDEVICE_LIBRARY})
set(AVDEVICE_INCLUDE_DIRS ${AVDEVICE_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(AVDEVICE DEFAULT_MSG AVDEVICE_LIBRARY AVDEVICE_INCLUDE_DIR)
