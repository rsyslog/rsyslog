***********************************************
omstdout: stdout output module (testbench tool)
***********************************************

===========================  ===========================================================================
**Module Name:**             **omstdout**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available Since:**         4.1.6
===========================  ===========================================================================


Purpose
=======

This module writes any messages that are passed to it to stdout. It
was developed for the rsyslog test suite. However, there may (limited)
exist some other usages. Please note we do not put too much effort on
the quality of this module as we do not expect it to be used in real
deployments. If you do, please drop us a note so that we can enhance
its priority!


Configuration
=============

.. note::

   Parameter names are case-insensitive.


Module Parameters
-----------------

none


Action Parameters
-----------------

template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_FileFormat", "no", "none"

Set the template which will be used for the output. If none is specified
the default will be used.


EnsureLFEnding
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$ActionOMStdoutEnsureLFEnding``"

Makes sure, that each message is written with a terminating LF. If the
message contains a trailing LF, none is added. This is needed for the
automated tests.


Configure statement
-------------------

This is used when building rsyslog from source.

./configure --enable-omstdout


Legacy parameter not adopted in the new style
---------------------------------------------

-  **$ActionOMStdoutArrayInterface**
   [Default: off]
   This setting instructs omstdout to use the alternate array based
   method of parameter passing. If used, the values will be output with
   commas between the values but no other padding bytes. This is a test
   aid for the alternate calling interface.


Examples
========

Minimum setup
-------------

The following sample is the minimum setup required to have syslog messages
written to stdout.

.. code-block:: none

   module(load="omstdout")
   action(type="omstdout")


Example 2
---------

The following sample will write syslog messages to stdout, using a template.

.. code-block:: none

   module(load="omstdout")
   action(type="omstdout" template="outfmt")


