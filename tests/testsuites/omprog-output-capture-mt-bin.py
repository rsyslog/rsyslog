# call this via "python[3] script name"

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

    sys.stdout.write(lineToStdout + "\n")
    sys.stderr.write(lineToStderr + "\n")

    # Flush stdout. In both Python 2 and Python 3, stdout is block-buffered when
    # redirected to a file/pipe. But be want each line to be written immediately,
    # because multiple processes are writing to the same pipe, and we do not want
    # lines to appear intermingled in the output file. (The flush will cause a
    # single 'write' syscall, since the size of the block buffer is generally
    # greater than PIPE_BUF.)
    sys.stdout.flush()

    # Flush stderr. This is not necessary in Python 2, since stderr is unbuffered
    # (and therefore each write will be written immediately and atomically to the
    # pipe). However, in Python 3 stderr is block-buffered when redirected to a
    # file/pipe. (Note: in future versions of Python 3, stderr could change to
    # line-buffered; see https://bugs.python.org/issue13601.)
    sys.stderr.flush()

    logLine = sys.stdin.readline()
