
bin_PROGRAMS = yarnb

SOURCES_YARNB = \
	main.c

HEADERS_YARNB =

yarnb_SOURCES = $(SOURCES_YARNB) $(HEADERS_YARNB)

AM_CFLAGS = $(YARN_CFLAGS_DIAG) $(YARN_CFLAGS_LIB)
AM_CPPFLAGS = -I$(srcdir)/../../libyarn/src
LDADD = ../../libyarn/src/libyarn.a $(YARN_LIBADD)
