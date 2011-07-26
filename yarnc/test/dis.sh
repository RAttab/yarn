#! /bin/bash

if [ -z "$1" ]; then
    echo "No input program."
    exit 1
fi

OUT_NAME=Debug+Asserts
BIN_PATH=~/code/llvm/$OUT_NAME/bin

PLAIN_BC=plain-dis.bc
PLAIN_LL=plain-dis.ll

OPT_BC=opt-dis.bc
OPT_LL=opt-dis.ll

YARN_BC=yarn-dis.bc
YARN_LL=yarn-dis.ll

clang -emit-llvm -O0 $1 -c -o $PLAIN_BC

$BIN_PATH/opt -O3 $PLAIN_BC -o $OPT_BC
echo $BIN_PATH/opt -load=../$OUT_NAME/lib/LLVMYarnc.so -loopsimplify -lcssa -yarn-loop -debug-pass=Structure $OPT_BC -o $YARN_BC
$BIN_PATH/opt -load=../$OUT_NAME/lib/LLVMYarnc.so -loopsimplify -lcssa -yarn-loop -debug-pass=Structure $OPT_BC -o $YARN_BC

llvm-dis $PLAIN_BC -o $PLAIN_LL
llvm-dis $OPT_BC -o $OPT_LL
llvm-dis $YARN_BC -o $YARN_LL

exit