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
Sets the maximum number of sessions supported by the listener.

.. note::

   Due to a long-standing bug, the configured limit is not forwarded to the
   underlying TCP listener. Regardless of the value set here, the listener
   currently enforces the built-in default of 200 sessions.

Input usage
-----------
.. _imgssapi.parameter.input.inputgssservermaxsessions-usage:

.. code-block:: rsyslog

   module(load="imgssapi")
   $InputGSSServerMaxSessions 200

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _imgssapi.parameter.legacy.inputgssservermaxsessions:

- $InputGSSServerMaxSessions — maps to InputGSSServerMaxSessions (status: legacy)

.. index::
   single: imgssapi; $InputGSSServerMaxSessions
   single: $InputGSSServerMaxSessions

See also
--------
See also :doc:`../../configuration/modules/imgssapi`.
