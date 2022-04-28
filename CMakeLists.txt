set(LLVM_LINK_COMPONENTS support)
file(GLOB_RECURSE sources *.cpp)
add_clang_executable(meta
  ${sources}
  )
target_link_libraries(meta
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
  )