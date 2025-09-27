.. _prop-system-myhostname:
.. _properties.system.myhostname:

$myhostname
===========

.. index::
   single: properties; $myhostname
   single: $myhostname

.. summary-start

Returns the local host name as rsyslog knows it.

.. summary-end

This property belongs to the **System Properties** group.

:Name: $myhostname
:Category: System Properties
:Type: string

Description
-----------
The name of the current host as it knows itself (probably useful for filtering in
a generic way).

Usage
-----
.. _properties.system.myhostname-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%$myhostname%")

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
