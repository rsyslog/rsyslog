.. _param-mmdarwin-fields:
.. _mmdarwin.parameter.input.fields:

fields
======

.. index::
   single: mmdarwin; fields
   single: fields

.. summary-start

Defines the array of values that mmdarwin forwards to Darwin as parameters.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdarwin`.

:Name: fields
:Scope: input
:Type: array
:Default: input=none
:Required?: yes
:Introduced: at least 8.x, possibly earlier

Description
-----------
Array containing values to be sent to Darwin as parameters.

Two types of values can be set:

* if it starts with a bang (:json:`"!"`), mmdarwin will search in the JSON-parsed log line the associated value. You can search in subkeys as well: just add a bang to go to a deeper level.
* otherwise, the value is considered static, and will be forwarded directly to Darwin.

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

and the :json:`"fields"` array:

.. code-block:: none

   ["!from", "!data!status", "rsyslog"]

The parameters sent to Darwin would be :json:`"192.168.1.42"`, :json:`true` and :json:`"rsyslog"`.

.. note::
    The order of the parameters is important. Thus, you have to be careful when providing the fields in the array.
    Refer to `Darwin documentation`_ to see what each filter requires as parameters.

.. _`Darwin documentation`: https://github.com/VultureProject/darwin/wiki

Input usage
-----------
.. _param-mmdarwin-input-fields:
.. _mmdarwin.parameter.input.fields-usage:

.. code-block:: rsyslog

   action(type="mmdarwin" fields=["!from", "!data!status", "rsyslog"])

See also
--------
See also :doc:`../../configuration/modules/mmdarwin`.
