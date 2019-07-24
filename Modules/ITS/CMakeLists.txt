# ---- Library ----

add_library(QcITS src/ITSTask.cxx)

target_sources(QcITS PRIVATE)

target_include_directories(
  QcITS
  PUBLIC $<INSTALL_INTERFACE:include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(QcITS PUBLIC QualityControl)

install(TARGETS QcITS
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ---- ROOT dictionary ----

add_root_dictionary(QcITS
                    HEADERS include/ITS/ITSTask.h
                    LINKDEF include/ITS/LinkDef.h
                    BASENAME QcITS)

# ---- Test(s) ----

add_executable(testQcITS test/testITS.cxx)
target_link_libraries(testQcITS PRIVATE QcITS Boost::unit_test_framework)
add_test(NAME testQcITS COMMAND testQcITS)
set_tests_properties(testQcITS PROPERTIES TIMEOUT 60)

# ---- Executables ----

set(EXE_SRCS src/runITS.cxx)

set(EXE_NAMES o2-qc-run-its)

list(LENGTH EXE_SRCS count)
math(EXPR count "${count}-1")
foreach(i RANGE ${count})
  list(GET EXE_SRCS ${i} src)
  list(GET EXE_NAMES ${i} name)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE QualityControl CURL::libcurl)
endforeach()

# ---- Extra scripts ----

install(FILES its.json DESTINATION etc)

get_property(dirs
             DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
             PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
  message(STATUS "dir='${dir}'")
endforeach()
