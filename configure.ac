#AC_MSG_ERROR(desc, [exit_code])
#AC_MSG_WARN(desc)
#AC_DEFINE(var, value, desc) => outputs #define in config.h
#AC_SUBST(var, [value]) => defines $(VAR) as value in makefile.

# Prelude
AC_INIT([yarn], [1.0], [not.now@maybe.later.com])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])

AC_CONFIG_SRCDIR([libyarn/src/yarn.c]) # Sanity check?

#Not sure why I have to escape the spaces...
AC_SUBST([YARN_CFLAGS_DIAG], [-Wall\ -Wextra\ -Werror\ -pedantic])
AC_SUBST([YARN_CFLAGS_LIB], [-D\ _GNU_SOURCE=1\ -pthread])
AC_SUBST([YARN_LIBADD], [-lrt])

# Check for programs
# AC_CHECK_PROGS([TAR], [tar gtar], [:]) 
# if test "$TAR" = ":"; then ...
AC_PROG_CC
AC_PROG_RANLIB
#AM_PROC_CC_C_0


# PKG_CHECK_MODULES([CHECK], [check >= 0.9.4]) 


# Check for libraries
# AC_CHECK_LIB(lib, func, [act-true], [act-false])

# Check for header files
AC_HEADER_STDC
AC_HEADER_ASSERT

AC_CHECK_HEADERS([stddef.h stdint.h stdlib.h sys/time.h unistd.h, stdbool.h])

# Check for typedef, structs and comp carac
AC_PROG_CC_C99
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE


#Output files
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([Makefile 
		libyarn/Makefile 
		libyarn/src/Makefile 
		libyarn/test/Makefile 
		yarnb/Makefile 
		yarnb/src/Makefile])

AC_OUTPUT
