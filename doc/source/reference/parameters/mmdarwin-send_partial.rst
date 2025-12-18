.. _param-mmdarwin-send_partial:
.. _mmdarwin.parameter.input.send_partial:

send_partial
============

.. index::
   single: mmdarwin; send_partial
   single: send_partial

.. summary-start

Controls whether mmdarwin calls Darwin when some fields are missing from the
message.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdarwin`.

:Name: send_partial
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Controls whether to send data to Darwin if not all ``"fields"`` could be
found in the message.
Darwin filters typically require a strict set of parameters and may not process
incomplete data, so leaving this setting at ``"off"`` is recommended unless
you have verified the filter accepts missing fields.

For example, for the following log line:

.. code-block:: json

   {
       "from": "192.168.1.42",
       "date": "2012-12-21 00:00:00",
       "status": "200",
       "data": {
           "status": true,
           "message": "Request processed correctly"
       }
   }

and the ``"fields"`` array:

.. code-block:: none

   ["!from", "!data!status", "!this!field!is!not!in!message"]

the third field won't be found, so the call to Darwin will be dropped.

Input usage
-----------
.. _param-mmdarwin-input-send_partial-usage:
.. _mmdarwin.parameter.input.send_partial-usage:

.. code-block:: rsyslog

   action(type="mmdarwin" sendPartial="on")

See also
--------
See also :doc:`../../configuration/modules/mmdarwin`.
