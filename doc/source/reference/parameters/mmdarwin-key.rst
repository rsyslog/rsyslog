.. _param-mmdarwin-key:
.. _mmdarwin.parameter.input.key:

key
===

.. index::
   single: mmdarwin; key
   single: key

.. summary-start

Stores the Darwin score in the specified JSON field.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdarwin`.

:Name: key
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: at least 8.x, possibly earlier

Description
-----------
The key name to use to store the returned data.

For example, given the following log line:

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

and the ``"certitude"`` key, the enriched log line would be:

.. code-block:: json
   :emphasize-lines: 9

   {
       "from": "192.168.1.42",
       "date": "2012-12-21 00:00:00",
       "status": "200",
       "data": {
           "status": true,
           "message": "Request processed correctly"
       },
       "certitude": 0
   }

where ``"certitude"`` represents the score returned by Darwin.

Input usage
-----------
.. _param-mmdarwin-input-key-usage:
.. _mmdarwin.parameter.input.key-usage:

.. code-block:: rsyslog

   action(type="mmdarwin" key="certitude")

See also
--------
See also :doc:`../../configuration/modules/mmdarwin`.
