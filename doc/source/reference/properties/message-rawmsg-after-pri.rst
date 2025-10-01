.. _prop-message-rawmsg-after-pri:
.. _properties.message.rawmsg-after-pri:

rawmsg-after-pri
================

.. index::
   single: properties; rawmsg-after-pri
   single: rawmsg-after-pri

.. summary-start

Contains the raw message content with the syslog PRI removed when present.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: rawmsg-after-pri
:Category: Message Properties
:Type: string

Description
-----------
Almost the same as :ref:`prop-message-rawmsg`, but the syslog PRI is removed. If no PRI was
present, ``rawmsg-after-pri`` is identical to :ref:`prop-message-rawmsg`. Note that the syslog
PRI is a header field that contains information on syslog facility and severity.
It is enclosed in greater-than and less-than characters, e.g. ``<191>``. This
field is often not written to log files, but usually needs to be present for the
receiver to properly classify the message. There are some rare cases where one
wants the raw message, but not the PRI. You can use this property to obtain
that. In general, you should know that you need this format, otherwise stay away
from the property.

Usage
-----
.. _properties.message.rawmsg-after-pri-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="rawmsg-after-pri")
   }

See also
--------
See :doc:`../../configuration/properties` for the category overview.
