#################################################################
#                         libcsvr-1.0                           #
#################################################################
# Copyright (C) 2022, Yuharsen Ergi.                            #
# All Rights Reserved.                                          #
# This file is distributed under the MIT License.               #
#                                                               #
# Author:  yuharsenergi                             2022-12-01  #
#################################################################

AC_PREREQ(2.59)

AC_INIT([libcsvr],[1.0],[yuharsenergi@gmail.com],,[https://github.com/yuharsenergi/libcsvr])

AC_PROG_CC

AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_SYSTEM

dnl Initialize automake
AM_INIT_AUTOMAKE

dnl this allows us specify individual liking flags for each target
AM_PROG_CC_C_O 

dnl Initialize Libtool
LT_INIT

dnl Check if Libtool is present
dnl Libtool is used for building share libraries 
AC_PROG_LIBTOOL

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(src/Makefile)
AC_CONFIG_FILES(include/Makefile)
AC_OUTPUT
