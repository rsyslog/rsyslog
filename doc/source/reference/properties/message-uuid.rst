.. _prop-message-uuid:
.. _properties.message.uuid:

uuid
====

.. index::
   single: properties; uuid
   single: uuid

.. summary-start

Provides a UUID assigned to the message when first accessed.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: uuid
:Category: Message Properties
:Type: string

Description
-----------
*Only Available if rsyslog is build with --enable-uuid*

A UUID for the message. It is not present by default, but will be created on
first read of the ``uuid`` property. Thereafter, in the local rsyslog instance,
it will always be the same value. This is also true if rsyslog is restarted and
messages stayed in an on-disk queue.

Note well: the UUID is **not** automatically transmitted to remote syslog
servers when forwarding. If that is needed, a special template needs to be
created that contains the uuid. Likewise, the receiver must parse that UUID
from that template.

The ``uuid`` property is most useful if you would like to track a single
message across multiple local destination. An example is messages being written
to a database as well as to local files.

Usage
-----
.. _properties.message.uuid-usage:

.. code-block:: rsyslog

   template(name="show-uuid" type="string" string="%uuid%")

Notes
~~~~~
Remember to add the UUID to forwarding templates if downstream systems must
receive it.

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
