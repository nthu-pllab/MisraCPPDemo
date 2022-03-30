set(LLVM_EXPORTED_SYMBOL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/First.exports)

include_directories(include)
include_directories(include/visitor)

set (CMAKE_CXX_STANDARD 14)
add_subdirectory(lib/helper)
add_subdirectory(lib/visitors)
add_subdirectory(lib/plugin)

add_llvm_loadable_module(MisracppChecker
	MainRegister.cpp
	PLUGIN_TOOL 
    clang
    )

target_link_libraries(MisracppChecker plugin -Wl,--whole-archive visitors -Wl,--no-whole-archive helper)
