include_directories(${CLANG_INCLUDE_DIRS})
add_llvm_library(plugin STATIC
    IndexConsumer.cpp  
    MisraConsumer.cpp  
    MisraPlugin.cpp
)





