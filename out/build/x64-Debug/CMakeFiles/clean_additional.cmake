# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\Oran7MediaClient_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\Oran7MediaClient_autogen.dir\\ParseCache.txt"
  "Oran7MediaClient_autogen"
  )
endif()
