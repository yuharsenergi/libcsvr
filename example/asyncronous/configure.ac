dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(example-asyncronous, 1.0)

AC_CONFIG_MACRO_DIR([m4])
# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE()
AM_PROG_CC_C_O

dnl Initialize Libtool
LT_INIT

dnl Check if Libtool is present
dnl Libtool is used for building share libraries 
AC_PROG_LIBTOOL

AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(src/Makefile)
AC_OUTPUT
