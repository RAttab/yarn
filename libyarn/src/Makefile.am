AM_CFLAGS = $(YARN_CFLAGS_DIAG) $(YARN_CFLAGS_LIB) -I$(srcdir)/src
LIBADD = $(YARN_LIBADD)


lib_LIBRARIES = libyarn.a

YARN_SRC_LIST = \
	atomic bits helper timer timestamp \
	tpool pstore pmem \
	dependency epoch map yarn

#SOURCES_LIBYARN = $(wildcard *.c)
SOURCES_LIBYARN =  \
	atomic.c bits.c helper.c timer.c timestamp.c \
	tpool.c pstore.c pmem.c \
	dependency.c epoch.c map.c yarn.c

#INCLUDES_LIBYARN = $(wildcard yarn/*.h)
INCLUDE_LIBYARN = yarn.h \
	yarn/types.h yarn/timer.h yarn/dependency.h

#HEADERS_LIBYARN = $(wildcard *.h)
HEADERS_LIBYARN = $(INCLUDE_LIBYARN) \
	dbh.h \
	atomic.h bits.h helper.h timestamp.h \
	tpool.h pstore.h pmem.h \
	epoch.h map.h

libyarn_a_SOURCES = $(SOURCES_LIBYARN) $(HEADERS_LIBYARN)

include_HEADERS = yarn.h $(INCLUDES_LIBYARN)
