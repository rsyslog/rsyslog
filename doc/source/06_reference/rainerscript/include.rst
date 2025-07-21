****************************
The rsyslog include() object
****************************

The ``include()`` object is used to include configuration snippets
stored elsewhere into the configuration.

.. versionadded:: 8.33.0

.. note::

   This configuration option deprecates the older ``$IncludeConfig``
   |FmtObsoleteName| format directive.

How it Works
============

The rsyslog include object is modelled after the usual "include" directive
in programming and script languages (e.g. \#include in C).

If you are not familiar with this, compare it to copy and paste: whenever
rsyslog finds an include object, in copies the text from that include file
at the exact position where the include is specified and removes the include
text.

Now remember that rsyslog's configuration statements are depending on the
position inside the configuration. It is important if a statement occurs
before or after another one. This is similar how other configuration files
work and also the same concept that almost all programming and script
languages have.

If you use includes in rsyslog, you must think about the position at which
the include text is inserted. This is especially important if you use
statements like `stop`. If given at the wrong spot, they will not work as
intended.

If in doubt, or if you have issues, it probably is best NOT to use includes.
This makes it far more obvious to understand what happens. Once solved, you
may then change back to includes again.


Parameters
==========

.. note::

   Parameter names are case-insensitive.

.. warning::

   Only one of the ``file`` or ``text`` parameters may be specified for each
   ``include()`` object.


file
----

Name of file to be included. May include wildcards, in which case all
matching files are included (in order of file name sort order).


text
----

Text to be included. This is most useful when using backtick string
constants.


mode
----

Affects how missing files are to be handled:

- ``abort-if-missing``, with rsyslog aborting when the file is not present
- ``required`` *(default)*, with rsyslog emitting an error message but otherwise
  continuing when the file is not present
- ``optional``, which means non-present files will be skipped without notice

Examples
========

Include a required file
-----------------------

.. code-block:: none

   include(file="/path/to/include.conf")

.. note::

   Unless otherwise specified, files referenced by an ``include()`` object
   must be present, otherwise an error will be generated.


Include an optional file
------------------------

The referenced file will be used if found, otherwise no errors or warnings
will be generated regarding its absence.

.. code-block:: none
   :emphasize-lines: 3

   include(
      file="/path/to/include.conf"
      mode="optional"
   )


Include multiple files
----------------------

.. code-block:: none

   include(file="/etc/rsyslog.d/*.conf")

.. note::

   Unless otherwise specified, files referenced by an ``include()`` object
   must be present, otherwise an error will be generated.


Include an environment variable as configuration
------------------------------------------------

.. code-block:: none

   include(text=`echo $ENV_VAR`)


Include a file specified via an environment variable
----------------------------------------------------

.. code-block:: none

   include(file=`echo $ENV_VAR`)

.. note::

   Unless otherwise specified, files referenced by an ``include()`` object
   must be present, otherwise an error will be generated.


Include an optional file specified via an environment variable
--------------------------------------------------------------

.. code-block:: none
   :emphasize-lines: 3

   include(
      file=`echo $ENV_VAR`
      mode="optional"
   )
