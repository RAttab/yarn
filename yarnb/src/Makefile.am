
bin_PROGRAMS = yarnb

yarnb_SOURCES = main.c

AM_CFLAGS = $(YARN_CFLAGS_DIAG) $(YARN_CFLAGS_LIB)
AM_CPPFLAGS = -I$(srcdir)/../../libyarn/src
LDADD = ../../libyarn/src/libyarn.a $(YARN_LIBADD)