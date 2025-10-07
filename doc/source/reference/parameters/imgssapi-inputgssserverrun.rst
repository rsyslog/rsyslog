.. _param-imgssapi-inputgssserverrun:
.. _imgssapi.parameter.input.inputgssserverrun:

InputGSSServerRun
=================

.. index::
   single: imgssapi; InputGSSServerRun
   single: InputGSSServerRun

.. summary-start

Starts a dedicated GSSAPI syslog listener on the specified port.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imgssapi`.

:Name: InputGSSServerRun
:Scope: input
:Type: word
:Default: none
:Required?: no
:Introduced: 3.11.5

Description
-----------
Starts a GSSAPI server on the selected port. This listener runs independently
from the TCP server provided by other modules.

Input usage
-----------
.. _imgssapi.parameter.input.inputgssserverrun-usage:

.. code-block:: rsyslog

   module(load="imgssapi")
   $inputGSSServerRun 1514

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _imgssapi.parameter.legacy.inputgssserverrun:

- $inputGSSServerRun — maps to InputGSSServerRun (status: legacy)

.. index::
   single: imgssapi; $inputGSSServerRun
   single: $inputGSSServerRun

See also
--------
See also :doc:`../../configuration/modules/imgssapi`.
