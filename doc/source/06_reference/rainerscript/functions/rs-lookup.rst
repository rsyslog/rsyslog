********
lookup()
********

Purpose
=======

lookup(table_name_literal_string, key)

Lookup tables are a powerful construct to obtain *class* information based
on message content. It works on top of a data-file which maps key (to be looked
up) to value (the result of lookup).

The idea is to use a message properties (or derivatives of it) as an index
into a table which then returns another value. For example, $fromhost-ip
could be used as an index, with the table value representing the type of
server or the department or remote office it is located in.

**Read more about it here** :doc:`Lookup Tables<../../../03_configuration/lookup_tables>`


Example
=======

In the following example the hostname is looked up in the given table and
the corresponding value is returned.

.. code-block:: none

   lookup_table(name="host_bu" file="/var/lib/host_billing_unit_mapping.json")
   set $.bu = lookup("host_bu", $hostname);


