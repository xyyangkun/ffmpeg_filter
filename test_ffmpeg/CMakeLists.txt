###################################################################################
#sample code
add_executable(muxing muxing.c)
target_link_libraries(muxing ${LINK_LIB_LIST})
set_target_properties(muxing PROPERTIES  COMPILE_FLAGS "-DTEST")
