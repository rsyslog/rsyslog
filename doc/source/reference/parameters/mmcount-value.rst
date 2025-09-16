.. _param-mmcount-value:
.. _mmcount.parameter.module.value:

value
=====

.. index::
   single: mmcount; value
   single: value

.. summary-start

Counts only messages where the selected key equals the specified value.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmcount`.

:Name: value
:Scope: module
:Type: word
:Default: none (count every observed key value)
:Required?: no
:Introduced: 7.5.0

Description
-----------
Limits counting to a specific property value when combined with
:ref:`param-mmcount-key`. After :ref:`param-mmcount-appname` matches and
the requested key is present, mmcount compares the property's string
representation with ``value``. If they are equal, a dedicated counter is
incremented and stored in the message's ``mmcount`` field. Messages that
do not match are left unchanged. When ``value`` is not provided, mmcount
maintains separate counters for each property value instead.

Specifying ``value`` without :ref:`param-mmcount-key` has no practical
effect; in that case the module still counts messages per severity.

Module usage
------------

.. code-block:: rsyslog

   action(type="mmcount" appName="glusterfsd" key="!gf_code" value="9999")

See also
--------
See also :doc:`../../configuration/modules/mmcount`.
