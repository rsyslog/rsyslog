.. _prop-message-source:
.. _properties.message.source:
.. _properties.alias.source:

source
======

.. index::
   single: properties; source
   single: source

.. summary-start

Refers to the same value as the ``hostname`` message property.

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

   template(name="show-source" type="string" string="%source%")

Aliases
~~~~~~~
- hostname — alias for source

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
