.. _param-imtcp-streamdriver-name:
.. _imtcp.parameter.module.streamdriver-name:
.. _imtcp.parameter.input.streamdriver-name:

StreamDriver.Name
=================

.. index::
   single: imtcp; StreamDriver.Name
   single: StreamDriver.Name

.. summary-start

Selects the network stream driver for all inputs using this module.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StreamDriver.Name
:Scope: module, input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=none, input=module parameter
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Selects :doc:`network stream driver <../../concepts/netstrm_drvr>`
for all inputs using this module.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-streamdriver-name:
.. _imtcp.parameter.module.streamdriver-name-usage:

.. code-block:: rsyslog

   module(load="imtcp" streamDriver.name="mydriver")

Input usage
-----------
.. _param-imtcp-input-streamdriver-name:
.. _imtcp.parameter.input.streamdriver-name-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" streamDriver.name="mydriver")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

