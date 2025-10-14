.. _param-immark-use-markflag:
.. _immark.parameter.module.use-markflag:

use.markFlag
=============

.. index::
   single: immark; use.markFlag
   single: use.markFlag

.. summary-start

Controls whether emitted mark messages carry the ``MARK`` flag bit.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/immark`.

:Name: use.markFlag
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: 8.2012.0

Description
-----------
When enabled, immark tags generated messages with the ``MARK`` flag.
This allows actions to filter or suppress them via settings such as the
``action.writeAllMarkMessages`` parameter. Disable the flag when mark
messages should be treated exactly like ordinary log entries.

Module usage
------------
.. _immark.parameter.module.use-markflag-usage:

.. code-block:: rsyslog

   module(load="immark" use.markFlag="off")

See also
--------

.. seealso::

   * :ref:`param-immark-interval`
   * :doc:`../../configuration/modules/immark`
