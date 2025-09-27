.. _prop-message-jsonmesg:
.. _properties.message.jsonmesg:

jsonmesg
========

.. index::
   single: properties; jsonmesg
   single: jsonmesg

.. summary-start

Provides the entire message object as a JSON representation.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: jsonmesg
:Category: Message Properties
:Type: JSON
:Introduced: 8.3.0

Description
-----------
*Available since rsyslog 8.3.0*

The whole message object as JSON representation. Note that the JSON string will
*not* include an LF and it will contain *all other message properties* specified
here as respective JSON containers. It also includes all message variables in the
"$!" subtree (this may be null if none are present).

This property is primarily meant as an interface to other systems and tools that
want access to the full property set (namely external plugins). Note that it
contains the same data items potentially multiple times. For example, parts of
the syslog tag will by contained in the rawmsg, syslogtag, and programname
properties. As such, this property has some additional overhead. Thus, it is
suggested to be used only when there is actual need for it.

Usage
-----
.. _properties.message.jsonmesg-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%jsonmesg%")

Notes
~~~~~
Use this property when consumers require the complete structured message in a
single JSON object.

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
