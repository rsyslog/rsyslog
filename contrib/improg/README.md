# improg: Program integration input module

| **Module Name:** | **improg**
|---------------|--------------
| **Authors:**  | Jean-Philippe Hilaire <jean-philippe.hilaire@pmu.fr> & Philippe Duveau <philippe.duveau@free.fr>

## Purpose
This module allows rsyslog to spawn an external command and consume message
from a pipe (stdout of the external process).

**Limitation:** `select()` seems not to support usage of `printf(...)` or
`fprintf(stdout,...)`. Only `write(STDOUT_FILENO,...)` seems to be efficient.

The imput module consume pipes form all external programs in a mono-threaded
`runInput` method. This means that data treatment is serial.

Optionally, the module manage the external program through keyword sent to
it using a another pipe.

An operational sample can be found @ "github.com/phduveau/jsonperfmon"

## Compile
To successfully compile improg module.

    ./configure --enable-improg ...

## Configuration Parameters / Action Parameters

### Binary
| Type | Mandatory | Value | Default 
|---|---|---|---
|string|yes|"command arguments..."|  

Command line : external program and arguments

### Tag
|Type|Mandatory|Value|Default 
|---|---|---|---
|string|no||none

The tag to be assigned to messages read from this file. If you would like to
see the colon after the tag, you need to include it when you assign a tag
value, like so: ``tag="myTagValue:"``.

### Facility
|Type|Mandatory|Value|Default 
|---|---|---|---
|string|no|"facility\|number"|"local0" 

The syslog facility to be assigned to messages read from this file. Can be
specified in textual form (e.g. ``local0``, ``local1``, ...) or as numbers (e.g.
16 for ``local0``). Textual form is suggested.

### Severity
|Type|Mandatory|Value|Default 
|---|---|---|---
|string|no|"severity\|number"|"notice"

The syslog severity to be assigned to lines read. Can be specified
in textual   form (e.g. ``info``, ``warning``, ...) or as numbers (e.g. 6
for ``info``). Textual form is suggested.

#### confirmMessages
| Type | Mandatory | Value | Default 
|---|---|---|---
|binary|no|"on\|off"|"on"

Specifies whether the external program needs feedback from rsyslog via stdin.
When this switch is set to "on", rsyslog confirms each received message.
This feature facilitates error handling: instead of having to implement a retry
logic, the external program can rely on the rsyslog queueing capabilities.
The program receives a line with the word ``ACK`` from its standard input.

Also, the program receives a ``STOP`` when rsyslog ask the module to stop.

#### signalOnClose
|Type|Mandatory|Value|Default 
|---|---|---|---
|binary|no|"on\|off"|"off"

Specifies whether a TERM signal must be sent to the external program before
closing it (when either the worker thread has been unscheduled, a restart
of the program is being forced, or rsyslog is about to shutdown).

#### closeTimeout
|Type|Mandatory|Value|Default 
|---|---|---|---
|number|no|"on\|off"|200

Specifies whether a KILL signal must be sent to the external program in case
it does not terminate within the timeout indicated by closeTimeout_
(when either the worker thread has been unscheduled, a restart of the program
is being forced, or rsyslog is about to shutdown).

#### killUnresponsive
|Type|Mandatory|Value|Default 
|---|---|---|---
|binary|no|"on\|off"|"on"

Specifies whether a KILL signal must be sent to the external program in case
it does not terminate within the timeout indicated by closeTimeout
(when either the worker thread has been unscheduled, a restart of the program
is being forced, or rsyslog is about to shutdown).

### Stop sequence

1. If `confirmMessages` is set to on, a `STOP` is written in stdin of the child.
2. If `signalOnClose` is set to "on", a TERM signal is sent to the child.
3. The pipes with the child process are closed (the child will receive EOF on stdin),
4. Then, rsyslog waits for the child process to terminate during closeTimeout, 
5. If the child has not terminated within the timeout, a KILL signal is sent to it.