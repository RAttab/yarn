
#
# Variables
#

BINS = simple sum

YARNC_BIN = ../Debug+Asserts/lib
YARNC_SO = $(YARNC_BIN)/LLVMYarnc.so
YARNC_A = $(YARNC_BIN)/LLVMYarnc.a

LIBYARN_BIN = ../../libyarn/src
LIBYARN = $(LIBYARN_BIN)/libyarn.a

CLANG_FLAGS = -O2
LINK_FLAGS = -pthread
YARNC_FLAGS = -loopsimplify -lcssa -yarn-loop


#
# Utility Rules
#

all : $(BINS)

# generates the llvm disassembly files.
dis : $(foreach a,$(BINS), s$(a).ll p$(a).ll)


clean :
	rm -f *.bc *.s *.ll *.out

.PHONY : all clean dis


#
# Program rules
#

#simple : simple.bc simple.s simple.ll psimple.ll


% : s%.out p%.out
	mv s$@.out $@
	mv p$@.out $@


#
# Pattern rules
#


s%.bc : %.c
	clang -emit-llvm -c $(CLANG_FLAGS) $< -o $@

p%.bc : s%.bc
	opt -load=$(YARNC_SO) $(YARNC_FLAGS) $< -o $@

%.ll : %.bc
	llvm-dis $< -o $@

%.s : %.bc
	llc $< -o $@

%.out : %.s
	gcc $(LINK_FLAGS) $< $(LIBYARN) -o $@
