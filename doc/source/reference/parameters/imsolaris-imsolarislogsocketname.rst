.. _param-imsolaris-imsolarislogsocketname:
.. _imsolaris.parameter.module.imsolarislogsocketname:

IMSolarisLogSocketName
======================

.. index::
   single: imsolaris; IMSolarisLogSocketName
   single: IMSolarisLogSocketName

.. summary-start

Specifies the Solaris log stream device imsolaris reads, defaulting to ``/dev/log``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imsolaris`.

.. note::

   This is a legacy global directive. The imsolaris module does not support the modern ``module()``/``input()`` syntax.

:Name: IMSolarisLogSocketName
:Scope: module
:Type: string (path)
:Default: module=/dev/log
:Required?: no
:Introduced: Not documented

Description
-----------
This is the name of the log socket (stream) to read. If not given, ``/dev/log`` is read.
Since directive names are case-insensitive, the canonical form ``$IMSolarisLogSocketName`` is recommended for readability.

Module usage
------------
.. _param-imsolaris-module-imsolarislogsocketname:
.. _imsolaris.parameter.module.imsolarislogsocketname-usage:

.. code-block:: rsyslog

   $ModLoad imsolaris
   $IMSolarisLogSocketName /var/run/rsyslog/solaris.log

See also
--------
See also :doc:`../../configuration/modules/imsolaris`.
