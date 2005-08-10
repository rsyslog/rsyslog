# Makefile for rsyslog
# Copyright (C) 2004, 2005 Rainer Gerhards and Adiscon GmbH
# This is the distro-specifc part of the Makefile.
# This makefile here should be suitable for all flavours
# of linux.
# For details, see http://www.rsyslog.com/doc

#############################################################
#                       USER SETTINGS                       #
#                       -------------                       #
# The following lines allow you to customize the way        #
# rsyslog is build. All variables take a value of 0 or zero #
# with 1 meaning true and 0 meaning false (in most cases    #
# equivalent to "disabled"). If you need to customize any-  #
# thing do it here - and stay away from all other parts     #
# of this file!                                             #
#                                                           #
# IMPORTANT: after you have made changes, run "make clean"  #
#            before any other command!                      #
#############################################################

# Enable large file support (typically on, not needed for
# 64 bit operating systems)
FEATURE_LARGEFILE=1

# Enable database support (off by default, must be turned
# on when support for MySQL is desired).
FEATURE_DB=0

#############################################################
#                  END OF USER SETTINGS                     #
#                  --------------------                     #
# DO NOT MAKE ANY MODIFICATIONS BELOW THIS POINT!           #
#############################################################

INSTALL = install
BINDIR = /usr/sbin
MANDIR = /usr/share/man

# now comes the evaluation of the FEATURE_* settings
ifeq ($(strip $(FEATURE_LARGEFILE)), 0)
  NOLARGEFILE = -DNOLARGEFILE
endif

ifeq ($(strip $(FEATURE_DB)), 1)
  WITHDB=-DWITHDB
endif

# Include MySQL client lib if DB is selected
ifdef WITHDB
LIBS = -lmysqlclient -L/usr/local/lib/mysql 
endif

VPATH = ../
include ../Makefile
