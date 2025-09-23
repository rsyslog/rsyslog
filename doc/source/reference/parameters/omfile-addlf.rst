.. _param-omfile-addlf:
.. _omfile.parameter.module.addlf:

addLF
=====

.. index::
   single: omfile; addLF
   single: addLF

.. summary-start

Ensures each written record ends with an LF by appending one when the message is missing it.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: addLF
:Scope: module, action
:Type: boolean
:Default: module=off; action=inherits module
:Required?: no
:Introduced: 8.2410.0

Description
-----------

When enabled, the omfile action checks whether a rendered message already
terminates with an LF (line feed) before writing it. If not, omfile appends an
LF so that every record ends with one, regardless of the template that produced
the message. The additional byte is added transparently even when compression or
cryptographic providers are enabled.

When configured on the module, the setting becomes the default for all omfile
actions that do not set ``addLF`` explicitly.

Module usage
------------

.. _param-omfile-module-addlf:
.. _omfile.parameter.module.addlf-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" addLF="on")

Action usage
------------

.. _param-omfile-action-addlf:
.. _omfile.parameter.action.addlf:
.. code-block:: rsyslog

   action(type="omfile" file="/var/log/messages" addLF="on")

See also
--------

See also :doc:`../../configuration/modules/omfile`.
