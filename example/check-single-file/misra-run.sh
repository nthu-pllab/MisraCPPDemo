#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
LLVM_BUILD=$SCRIPT_DIR/../../../../../../build
CLANG=$LLVM_BUILD/bin/clang
MISRA_CHECKER=$LLVM_BUILD/lib/MisracppChecker.so

if [ ! -f $CLANG ]; then
    echo "clang not found, specified LLVM_BUILD for $0"
    echo "expected clang path: $CLANG"
    exit 1
fi

if [ ! -f $MISRA_CHECKER ]; then
    echo "MisracppChecker.so not found, specified LLVM_BUILD for $0"
    echo "expected MisracppChecker.so path: $MISRA_CHECKER"
    exit 1
fi

cmd="$CLANG -fsyntax-only -Xclang -load -Xclang $MISRA_CHECKER"
cmd="$cmd -Xclang -plugin -Xclang Misra-Checker"
plugin="-Xclang -plugin-arg-Misra-Checker -Xclang"

for i in "${@:1:($#-1)}"
do
    cmd="$cmd $plugin $i"
done

cmd="$cmd ${!#}"
echo $cmd
$cmd
