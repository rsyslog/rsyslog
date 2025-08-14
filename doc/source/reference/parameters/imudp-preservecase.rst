.. _param-imudp-preservecase:
.. _imudp.parameter.module.preservecase:

PreserveCase
============

.. index::
   single: imudp; PreserveCase
   single: PreserveCase

.. summary-start

Preserves the exact letter case of the ``fromhost`` value instead of lowering it.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: PreserveCase
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.37.0

Description
-----------
Controls the case of the ``fromhost`` property. When set to ``on`` the host name
is kept exactly as received, for example ``Host1.Example.Org``. The default
``off`` keeps historical behavior of lowercasing for backward compatibility.

Module usage
------------
.. _param-imudp-module-preservecase:
.. _imudp.parameter.module.preservecase-usage:
.. code-block:: rsyslog

   module(load="imudp" PreserveCase="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.

