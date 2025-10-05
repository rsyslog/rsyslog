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
:Default: input=200
:Required?: no
:Introduced: 3.11.5

Description
-----------
Sets the maximum number of sessions supported by the listener.

Input usage
-----------
.. _param-imgssapi-input-inputgssservermaxsessions:
.. _imgssapi.parameter.input.inputgssservermaxsessions-usage:

.. code-block:: rsyslog

   module(load="imgssapi")
   $InputGSSServerMaxSessions 200

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imgssapi.parameter.legacy.inputgssservermaxsessions:

- $InputGSSServerMaxSessions — maps to InputGSSServerMaxSessions (status: legacy)

.. index::
   single: imgssapi; $InputGSSServerMaxSessions
   single: $InputGSSServerMaxSessions

See also
--------
See also :doc:`../../configuration/modules/imgssapi`.
