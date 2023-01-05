set(ALL_TESTS
   test_pipelines
)

add_library(pipelines SHARED ../../pipeline.cpp)

add_executable(test_pipelines test_pipelines.cpp)

foreach(target ${ALL_TESTS})
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  target_compile_features(${target} PUBLIC cxx_std_17)
  target_compile_options(${target} PRIVATE -Dprivate=public ${DEFAULT_COMPILER_OPTIONS_AND_WARNINGS})
  target_link_libraries(${target} PRIVATE pipelines Catch2::Catch2WithMain)
  add_test(NAME ${target} COMMAND ${target})
endforeach()
