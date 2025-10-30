.. _param-imrelp-flowcontrol:
.. _imrelp.parameter.input.flowcontrol:

flowControl
===========

.. index::
   single: imrelp; flowControl
   single: flowControl

.. summary-start

Fine-tunes RELP input flow control behavior between no, light, or full throttling.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: flowControl
:Scope: input
:Type: string
:Default: input=light
:Required?: no
:Introduced: 8.1911.0

Description
-----------
This parameter permits the fine-tuning of the flowControl parameter. Possible
values are "no", "light", and "full". With "light" being the default and
previously only value.

Changing the flow control setting may be useful for some rare applications, this
is an advanced setting and should only be changed if you know what you are doing.
Most importantly, **rsyslog block incoming data and become unresponsive if you
change flowcontrol to "full"**. While this may be a desired effect when
intentionally trying to make it most unlikely that rsyslog needs to lose/discard
messages, usually this is not what you want.

General rule of thumb: **if you do not fully understand what this description
here talks about, leave the parameter at default value**.

This part of the documentation is intentionally brief, as one needs to have deep
understanding of rsyslog to evaluate usage of this parameter. If someone has the
insight, the meaning of this parameter is crystal-clear. If not, that someone
will most likely make the wrong decision when changing this parameter away from
the default value.

Input usage
-----------
.. _param-imrelp-input-flowcontrol-usage:
.. _imrelp.parameter.input.flowcontrol-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" flowControl="light")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
