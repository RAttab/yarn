#AM_CONDITIONAL([WANT_CHECK], [test ...])
# if WANT_CHECK
#   bin_PROGRAMS += check_libyarn
# endif

AM_CFLAGS = $(YARN_CFLAGS_DIAG) $(YARN_CFLAGS_LIB) -I$(srcdir)/../src
LDADD = $(YARN_LIBADD) ../src/libyarn.a -lcheck


#check_PROGRAMS = check_libyarn
check_PROGRAMS = check_libyarn

#SOURCES_CHECK = $(wildcard *.c)
SOURCES_CHECK = \
	check_bits.c \
	check_map.c check_pmem.c check_pstore.c check_tpool.c \
	check_dependency.c check_epoch.c check_yarn.c \
	check_libyarn.c t_utils.c

#HEADERS_CHECK = $(wildcard *.h)
HEADERS_CHECK = \
	check_libyarn.h t_utils.h

check_libyarn_SOURCES = $(SOURCES_CHECK) $(HEADERS_CHECK)

