# -*- autoconf -*-

dnl Checks for TCL.
AC_DEFUN([ESNACC_CHECK_TCL],
  [AC_ARG_WITH([tcl],
              [AC_HELP_STRING([--with-tcl],
                              [enables TCL integration for c/c++ libraries])])
  AC_MSG_CHECKING([if tcl support is enabled])
  AM_CONDITIONAL([TCL_SUPPORTED], [test "with_tcl" == yes])
  if test -z "$with_tcl" || test "$with_tcl" == no; then
     AC_MSG_RESULT([no])
  else
     AC_MSG_RESULT([yes])
     hdr_test=false
     AC_CHECK_HEADER(tk.h, [], [])
     AC_DEFINE([TCL], [1], [TCL Support enabled])
  fi
  ])
