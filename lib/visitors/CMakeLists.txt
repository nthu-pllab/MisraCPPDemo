set(Checkers
    3_9_3.cpp
    5_0_10.cpp
    10_1_2.cpp
    14_8_1.cpp
    14_8_2.cpp
    15_3_2.cpp
    16_0_7.cpp
    )

add_llvm_library(visitors STATIC
    ${Checkers}
    StdLib.cpp
    DEPENDS
    helper)

#add_subdirectory(dev)
