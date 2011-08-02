################################################################################
#
# Makefile - Yarnc Tests
#
# Must be built using the -r makefile switch.
#
################################################################################


#
# Env Variables
#

BUILD = Debug+Asserts

BIN_SRC = simple sum
YARNC_BIN = ../$(BUILD)/lib
LIBYARN_BIN = ../../libyarn/src
LLVM = ~/code/llvm


#
# Build Flags
#

CLANG_FLAGS = -O2
LINK_FLAGS = -pthread
YARNC_FLAGS = -loopsimplify -lcssa -yarn-loop


#
# Derived Variables
#

BINS = $(foreach a, $(BIN_SRC), s$(a) p$(a))

YARNC_SO = $(YARNC_BIN)/LLVMYarnc.so
YARNC_A = $(YARNC_BIN)/LLVMYarnc.a
LIBYARN = $(LIBYARN_BIN)/libyarn.a

LLVM_BIN = $(LLVM)/$(BUILD)/bin

#
# Utility Rules
#

all : $(BINS)

# generates the llvm disassembly files.
dis : $(foreach a, $(BINS), $(a).ll)


clean :
	rm -f *.bc *.s *.ll $(BINS)

.PHONY : all clean dis
.SECONDARY : $(foreach a, $(BINS), $(a).bc $(a).out)



#
# Pattern rules
#


s%.bc : %.c
	clang -emit-llvm -c $(CLANG_FLAGS) $< -o $@

p%.bc : s%.bc
	$(LLVM_BIN)/opt -load=$(YARNC_SO) $(YARNC_FLAGS) $< -o $@

%.ll : %.bc
	$(LLVM_BIN)/llvm-dis $< -o $@

%.s : %.bc
	$(LLVM_BIN)/llc $< -o $@

% : %.s
	gcc $(LINK_FLAGS) $< $(LIBYARN) -o $@
