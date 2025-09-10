****************************************
improg: Program integration input module
****************************************

================  ==============================================================
**Module Name:**  **improg**
**Authors:**      Jean-Philippe Hilaire <jean-philippe.hilaire@pmu.fr> & Philippe Duveau <philippe.duveau@free.fr>
================  ==============================================================


Purpose
=======

This module allows rsyslog to spawn external command(s) and consume message
from pipe(s) (stdout of the external process).

**Limitation:** Be careful when you rely on stdio like `printf(...)` or
`fprintf(stdout,...)` - the buffering they apply can prevent your message
go out timely to improg and may make your process seem stuck. Either disable
buffering or be sure to do an `fflush()` when ready with the current output.

The input module consume pipes form all external programs in a mono-threaded
`runInput` method. This means that data treatments will be serialized.

Optionally, the module manage the external program through keyword sent to
it using a second pipe to stdin of the external process.

An operational sample in C can be found @ "github.com/pduveau/jsonperfmon"

Also a bash's script is provided as tests/improg-simul.sh. The `echo` and `read` (built-in) can be used to communicate with the module.
External commands can not be used to communicate. `printf` is unable to send data directly to the module but can used through a variable and `echo`.


Compile
=======

To successfully compile improg module.

    ./configure --enable-improg ...

Configuration Parameters
========================

Action Parameters
-----------------

Binary
^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "yes", "command arguments...",

Command line : external program and arguments

Tag
^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "yes", ,"none"

The tag to be assigned to messages read from this file. If you would like to
see the colon after the tag, you need to include it when you assign a tag
value, like so: ``tag="myTagValue:"``.

Facility
^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "facility\|number", "local0"

The syslog facility to be assigned to messages read from this file. Can be
specified in textual form (e.g. ``local0``, ``local1``, ...) or as numbers (e.g.
16 for ``local0``). Textual form is suggested.

Severity
^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "severity\|number", "notice"

The syslog severity to be assigned to lines read. Can be specified
in textual   form (e.g. ``info``, ``warning``, ...) or as numbers (e.g. 6
for ``info``). Textual form is suggested.

confirmMessages
^^^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "binary", "no", "on\|off", "on"

Specifies whether the external program needs feedback from rsyslog via stdin.
When this switch is set to "on", rsyslog confirms each received message.
This feature facilitates error handling: instead of having to implement a retry
logic, the external program can rely on the rsyslog queueing capabilities.
The program receives a line with the word ``ACK`` from its standard input.

Also, the program receives a ``STOP`` when rsyslog ask the module to stop.

signalOnClose
^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "binary", "no", "on\|off", "off"

Specifies whether a TERM signal must be sent to the external program before
closing it (when either the worker thread has been unscheduled, a restart
of the program is being forced, or rsyslog is about to shutdown).

closeTimeout
^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "number", "no", ,"200"

Specifies whether a KILL signal must be sent to the external program in case
it does not terminate within the timeout indicated by closeTimeout_
(when either the worker thread has been unscheduled, a restart of the program
is being forced, or rsyslog is about to shutdown).

killUnresponsive
^^^^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "binary", "no", "on\|off", "on"

Specifies whether a KILL signal must be sent to the external program in case
it does not terminate within the timeout indicated by closeTimeout
(when either the worker thread has been unscheduled, a restart of the program
is being forced, or rsyslog is about to shutdown).

Stop sequence
=============

1. If `confirmMessages` is set to on, a `STOP` is written in stdin of the child.
2. If `signalOnClose` is set to "on", a TERM signal is sent to the child.
3. The pipes with the child process are closed (the child will receive EOF on stdin),
4. Then, rsyslog waits for the child process to terminate during closeTimeout,
5. If the child has not terminated within the timeout, a KILL signal is sent to it.


