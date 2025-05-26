#!/usr/bin/env python
# Copyright (c) 2019 by Rainer Gerhards, released under ASL 2.0
import sys

rc=0
max_line_length=72 # github limit; actually lending towards smaller (50) is better
len_hash = 9 # we use 8-char hash plus space
f = open(sys.argv[1], "r")
for line in f:
   len_line = len(line) - len_hash
   if len_line > max_line_length:
       sys.stderr.write("This commit title is {} characters too long (maximum is {}, line len is {}):\n".format(
           len_line - max_line_length, max_line_length, len_line))
       sys.stderr.write(line)
       rc = 1

sys.exit(rc)
