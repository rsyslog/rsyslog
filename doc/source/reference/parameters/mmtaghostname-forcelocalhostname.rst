.. _param-mmtaghostname-forcelocalhostname:
.. _mmtaghostname.parameter.input.forcelocalhostname:

ForceLocalHostname
==================

.. index::
   single: mmtaghostname; ForceLocalHostname
   single: ForceLocalHostname

.. summary-start

Forces the message ``HOSTNAME`` to the rsyslog ``localHostName`` value.

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
This parameter forces the message's ``HOSTNAME`` field to the rsyslog value
``localHostName``. This is useful for setting a consistent hostname on
messages that may not have one, e.g. those received via ``imudp`` or
``imtcp``.
A common use case is with applications in auto-scaling environments
(e.g. AWS), where instances may have ephemeral hostnames. Forcing the
hostname to the rsyslog host's name can provide more meaningful and
consistent hostnames in logs.

Input usage
-----------
.. _mmtaghostname.parameter.input.forcelocalhostname-usage:

.. code-block:: rsyslog

   action(type="mmtaghostname"
          forceLocalHostname="on")

Notes
-----
- Legacy documentation referred to the type as ``Binary``;
  it is treated as boolean.

See also
--------
* :doc:`../../configuration/modules/mmtaghostname`
