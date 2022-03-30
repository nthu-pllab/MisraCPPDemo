set(Checker-dev
    lib/visitors/10_1.cpp
    lib/visitors/9_3.cpp
    #    lib/checkers/9_1.cpp
    lib/visitors/7_4.cpp
)

add_llvm_loadable_module(MisracppChecker-dev
    ${Plugin}
	${Checker-dev}
	PLUGIN_TOOL clang)

