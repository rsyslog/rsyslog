# Makefile for rsyslog
# Copyright (C) 2004, 2005 Rainer Gerhards and Adiscon GmbH
# This is the distro-specifc part of the Makefile.
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

# Enable large file support (typically on, not needed on
# 64 bit operating systems)
FEATURE_LARGEFILE=1

# Enable database support (off by default, must be turned
# on when support for MySQL is desired).
FEATURE_DB=1

# Enable regular expressions
FEATURE_REGEXP=1

# Enable RFC 3195 support (REQUIRES LIBLOGGING 0.6.0 or above!)
FEATURE_RFC3195=0

# Enable debug mode (much slower code)
FEATURE_DEBUG=0

#############################################################
#                  END OF USER SETTINGS                     #
#                  --------------------                     #
# DO NOT MAKE ANY MODIFICATIONS BELOW THIS POINT!           #
#############################################################

INSTALL = install
BINDIR = /usr/sbin
MANDIR = /usr/share/man


# now comes the evaluation of the FEATURE_* settings
.if $(FEATURE_LARGEFILE) == 0
  NOLARGEFILE = -DNOLARGEFILE
.endif

.if $(FEATURE_DB) == 1
  WITHDB=-DWITH_DB
.endif

.if $(FEATURE_REGEXP) == 1
  F_REGEXP=-DFEATURE_REGEXP
.endif

.if $(FEATURE_RFC3195) == 1
  F_RFC3195=-DFEATURE_RFC3195
.endif

.if $(FEATURE_DEBUG) == 0
  DBG=-DNDEBUG
.endif

# Include MySQL client lib if DB is selected
.ifdef WITHDB
LIBS = -lmysqlclient -L/usr/local/lib/mysql 
.endif

VPATH = ../
.include "../master.make"
