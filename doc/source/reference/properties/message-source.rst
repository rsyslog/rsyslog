.. _prop-message-source:
.. _properties.message.source:

source
======

.. index::
   single: properties; source
   single: source

.. summary-start

Provides the same hostname value as :ref:`prop-message-hostname`.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: source
:Category: Message Properties
:Type: string

Description
-----------
Alias for :ref:`prop-message-hostname`.

Usage
-----
.. _properties.message.source-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="source")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
