.. _param-imgssapi-inputgssservermaxsessions:
.. _imgssapi.parameter.input.inputgssservermaxsessions:

InputGSSServerMaxSessions
=========================

.. index::
   single: imgssapi; InputGSSServerMaxSessions
   single: InputGSSServerMaxSessions

.. summary-start

Sets the maximum number of concurrent sessions supported by the server.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imgssapi`.

:Name: InputGSSServerMaxSessions
:Scope: input
:Type: integer
:Default: 200
:Required?: no
:Introduced: 3.11.5

Description
-----------
Sets the maximum number of concurrent sessions supported by the listener.

Input usage
-----------
.. _imgssapi.parameter.input.inputgssservermaxsessions-usage:

.. code-block:: rsyslog

   module(load="imgssapi")
   $inputGssServerMaxSessions 200

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _imgssapi.parameter.legacy.inputgssservermaxsessions:

- $inputGssServerMaxSessions — maps to InputGSSServerMaxSessions (status: legacy)

.. index::
   single: imgssapi; $inputGssServerMaxSessions
   single: $inputGssServerMaxSessions

See also
--------
See also :doc:`../../configuration/modules/imgssapi`.
