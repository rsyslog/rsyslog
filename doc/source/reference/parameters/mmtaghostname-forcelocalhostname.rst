.. _param-mmtaghostname-forcelocalhostname:
.. _mmtaghostname.parameter.input.forcelocalhostname:

ForceLocalHostname
==================

.. index::
   single: mmtaghostname; ForceLocalHostname
   single: ForceLocalHostname

.. summary-start

Forces the message hostname to the rsyslog local host name.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmtaghostname`.

:Name: ForceLocalHostname
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 7.0, possibly earlier

Description
-----------
This attribute forces the message's ``HOSTNAME`` field to the rsyslog value
``localHostName``. It allows setting a valid hostname for messages received
from local applications through imudp or imtcp.

Input usage
-----------
.. _mmtaghostname.parameter.input.forcelocalhostname-usage:

.. code-block:: rsyslog

   action(type="mmtaghostname"
          forceLocalHostname="on")

Notes
-----
- Legacy documentation referred to the type as ``Binary``; it is treated as boolean.

See also
--------
See also :doc:`../../configuration/modules/mmtaghostname`.
