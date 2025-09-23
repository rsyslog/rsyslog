.. _param-mmjsonparse-container:
.. _mmjsonparse.parameter.input.container:

container
=========

.. index::
   single: mmjsonparse; container
   single: container

.. summary-start

Sets the JSON container path where parsed properties are stored.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsonparse`.

:Name: container
:Scope: input
:Type: string
:Default: $!
:Required?: no
:Introduced: 6.6.0

Description
-----------
Specifies the JSON container (path) under which parsed elements should be
placed. By default (``$!``), all parsed properties are merged into the root of
the message properties.

You can specify a different container:

* To place data in a subtree of message properties, for example
  ``container="$!json"``.
* To use a local variable, for example ``container="$.parses"``.
* To use a global variable, for example ``container="$/parses"``.

Input usage
-----------
.. _mmjsonparse.parameter.input.container-usage:

.. code-block:: rsyslog

   action(type="mmjsonparse" container="$.parses")

See also
--------
See also :doc:`../../configuration/modules/mmjsonparse`.
