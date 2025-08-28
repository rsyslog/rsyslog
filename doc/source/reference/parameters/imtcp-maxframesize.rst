.. _param-imtcp-maxframesize:
.. _imtcp.parameter.module.maxframesize:
.. _imtcp.parameter.input.maxframesize:

MaxFrameSize
============

.. index::
   single: imtcp; MaxFrameSize
   single: MaxFrameSize

.. summary-start

Sets the maximum frame size in octet-counted mode before switching to octet stuffing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: MaxFrameSize
:Scope: module, input
:Type: integer
:Default: module=200000, input=module parameter
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
When in octet counted mode, the frame size is given at the beginning
of the message. With this parameter the max size this frame can have
is specified and when the frame gets to large the mode is switched to
octet stuffing.
The max value this parameter can have was specified because otherwise
the integer could become negative and this would result in a
Segmentation Fault. (Max Value = 200000000)

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-maxframesize:
.. _imtcp.parameter.module.maxframesize-usage:

.. code-block:: rsyslog

   module(load="imtcp" maxFrameSize="...")

Input usage
-----------
.. _param-imtcp-input-maxframesize:
.. _imtcp.parameter.input.maxframesize-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" maxFrameSize="...")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

