.. _prop-message-source:
.. _properties.message.source:
.. _properties.alias.source:

source
======

.. index::
   single: properties; source
   single: source

.. summary-start

Provides the same hostname value as the ``hostname`` property.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: source
:Category: Message Properties
:Type: string
:Aliases: hostname

Description
-----------
Alias for ``hostname``.

Usage
-----
.. _properties.message.source-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%source%")

Aliases
~~~~~~~
- hostname — alias for source

See also
--------
See :doc:`../../configuration/properties` for the category overview.
