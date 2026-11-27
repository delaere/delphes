# EVE7 (web-based) event display, built when ROOT provides the ROOTEve library

include_directories(
  ${CMAKE_SOURCE_DIR}
  ${ROOT_INCLUDE_DIRS}
  ${DelphesExternals_INCLUDE_DIR}
)

file(GLOB sources *.cc)
file(GLOB headers *.h)
list(REMOVE_ITEM headers ${CMAKE_CURRENT_SOURCE_DIR}/Display7LinkDef.h)

# the detector geometry builder does not depend on EVE: share it with display/
list(APPEND sources ${CMAKE_SOURCE_DIR}/display/Delphes3DGeometry.cc)

DELPHES_GENERATE_DICTIONARY(Display7Dict ${headers} ${CMAKE_SOURCE_DIR}/display/Delphes3DGeometry.h LINKDEF Display7LinkDef.h)

add_library(display7 OBJECT ${sources} Display7Dict.cxx)

# install headers
install(FILES ${headers} DESTINATION include/display7)

# install pcms
install(FILES
    ${PROJECT_BINARY_DIR}/display7/libDisplay7Dict_rdict.pcm
    DESTINATION lib)
