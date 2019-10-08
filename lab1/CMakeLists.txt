set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )

add_clang_executable(lab1
  ASTInterpreter.cpp
  )

target_link_libraries(lab1
  PRIVATE
  clangAST
  clangBasic
  clangFrontend
  clangTooling
  )

install(TARGETS lab1
  RUNTIME DESTINATION bin)
