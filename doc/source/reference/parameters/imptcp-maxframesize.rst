.. _param-imptcp-maxframesize:
.. _imptcp.parameter.input.maxframesize:

MaxFrameSize
============

.. index::
   single: imptcp; MaxFrameSize
   single: MaxFrameSize

.. summary-start

Sets the maximum frame size when using octet counted mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: MaxFrameSize
:Scope: input
:Type: integer
:Default: input=200000
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
When in octet counted mode, the frame size is given at the beginning
of the message. With this parameter the max size this frame can have
is specified and when the frame gets too large the mode is switched to
octet stuffing. The max value this parameter can have was specified
because otherwise the integer could become negative and this would
result in a Segmentation Fault. (Max Value: 200000000)

Input usage
-----------
.. _param-imptcp-input-maxframesize:
.. _imptcp.parameter.input.maxframesize-usage:

.. code-block:: rsyslog

   input(type="imptcp" maxFrameSize="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
