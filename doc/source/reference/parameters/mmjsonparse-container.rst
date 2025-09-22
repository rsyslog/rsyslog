.. _param-mmjsonparse-container:
.. _mmjsonparse.parameter.module.container:

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
:Scope: module
:Type: string
:Default: module=$!
:Required?: no
:Introduced: at least 6.6.0, possibly earlier

Description
-----------
Specifies the JSON container (path) under which parsed elements should be
placed. By default, all parsed properties are merged into the root of message
properties.

You can place parsed data under a subtree instead. You can also store them in
local variables by setting ``container="$."``.

Module usage
------------
.. _param-mmjsonparse-module-container:
.. _mmjsonparse.parameter.module.container-usage:

.. code-block:: rsyslog

   action(type="mmjsonparse" container="$.parses")

See also
--------
See also :doc:`../../configuration/modules/mmjsonparse`.
