.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Assign a syslog facility to Docker logs using standard names or numeric facility ordinals.
   :keywords: rsyslog, imdocker, DefaultFacility, syslog facility, facility ordinal

.. _param-imdocker-defaultfacility:
.. _imdocker.parameter.module.defaultfacility:

DefaultFacility
===============

.. index::
   single: imdocker; DefaultFacility
   single: DefaultFacility

.. summary-start

Syslog facility assigned to received messages; default ``user``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: DefaultFacility
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=user
:Required?: no
:Introduced: 8.41.0

Description
-----------
The syslog facility to be assigned to log messages. Standard facility names
such as ``kern``, ``user``, ``mail``, ``daemon``, and ``local0`` through
``local7`` are accepted without regard to case. Textual names are recommended.

Numeric strings are facility **ordinals**, not PRI-encoded facility values.
For example, ``DefaultFacility="user"`` and ``DefaultFacility="1"`` both
select facility 1, while ``DefaultFacility="8"`` selects facility 8.
Similarly, ``"mail"`` selects facility 2 and ``"16"`` selects facility 16
(``local0``). The facility contribution to a message PRI is the ordinal
multiplied by 8; the severity is then added. Keep numeric values quoted in
RainerScript, as in these examples.

Module usage
------------
.. _param-imdocker-module-defaultfacility:
.. _imdocker.parameter.module.defaultfacility-usage:

.. code-block:: rsyslog

   module(load="imdocker" DefaultFacility="...")

Notes
-----
- Numeric facility values are accepted but textual names are recommended.
- See https://en.wikipedia.org/wiki/Syslog.

See also
--------
See also :doc:`../../configuration/modules/imdocker`.

