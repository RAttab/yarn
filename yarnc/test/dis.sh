#! /bin/bash

if [ -z "$1" ]; then
    echo "No input program."
    exit 1
fi

PLAIN_BC=dis.bc
PLAIN_LL=dis.ll

OPT_BC=dis-opt.bc
OPT_LL=dis-opt.ll

clang -emit-llvm -O0 $1 -c -o $PLAIN_BC

opt -O3 $PLAIN_BC -o $OPT_BC
opt -loopsimplify $OPT_BC -o $OPT_BC

llvm-dis $PLAIN_BC -o $PLAIN_LL
llvm-dis $OPT_BC -o $OPT_LL

rm $PLAIN_BC $OPT_BC

exit