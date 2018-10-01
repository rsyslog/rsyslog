#!/usr/bin/env python

import sys
import os

lineLength = int(sys.argv[1])
linePrefix = "[{0:09d}] ".format(os.getpid())

logLine = sys.stdin.readline()
while logLine:
    logLine = logLine.strip()
    numRepeats = int(lineLength / len(logLine))
    lineToStdout = (linePrefix + "[stdout] " + logLine*numRepeats)[:lineLength]
    lineToStderr = (linePrefix + "[stderr] " + logLine*numRepeats)[:lineLength]

    # Write to stdout without flushing. Since stdout is block-buffered when redirected to a pipe,
    # and multiple processes are writing to the pipe, this will cause intermingled lines in the
    # output file. However, we avoid this by executing this script with 'stdbuf -oL' (see the
    # test code), which forces line buffering for stdout. We could alternatively call
    # sys.stdout.flush() after the write (this also causes a single 'write' syscall, since the
    # size of the block buffer is generally greater than PIPE_BUF).
    sys.stdout.write(lineToStdout + "\n")

    # Write to stderr using a single write. Since stderr is unbuffered, each write will be
    # written immediately (and atomically) to the pipe.
    sys.stderr.write(lineToStderr + "\n")

    # Note (FTR): In future versions of Python3, stderr will possibly be line buffered (see
    # https://bugs.python.org/issue13601). The previous write will also be atomic in this case.
    # Note (FTR): When writing to stderr using the Python logging module, it seems that line
    # buffering is also used (although this could depend on the Python version).

    logLine = sys.stdin.readline()
