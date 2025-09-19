.. _param-mmcount-key:
.. _mmcount.parameter.input.key:

key
===

.. index::
   single: mmcount; key
   single: key

.. summary-start

Names the JSON property whose values are counted on matching messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmcount`.

:Name: key
:Scope: input
:Type: word
:Default: none (count messages per severity)
:Required?: no
:Introduced: 7.5.0

Description
-----------
Directs mmcount to evaluate a structured property on each message that
matches :ref:`param-mmcount-appname`. The value should be the RainerScript
variable name used to access the JSON property (e.g., ``!gf_code``).

If the named property is absent, mmcount leaves the message unchanged and
no counter is modified. When the property exists, its value is converted
into a string (JSON ``null`` becomes an empty string) and used to select
which counter to increment. Without :ref:`param-mmcount-value`, mmcount
creates a distinct counter for every observed value and writes the
running total for that value into the message's ``mmcount`` property.

Input usage
-----------
.. _param-mmcount-key-usage:
.. _mmcount.parameter.input.key-usage:

.. code-block:: rsyslog

   action(type="mmcount" appName="glusterd" key="!gf_code")

See also
--------
See also :doc:`../../configuration/modules/mmcount`.

