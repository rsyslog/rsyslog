.. _param-imtcp-allowedsender:
.. _imtcp.parameter.module.allowedsender:
.. _imtcp.parameter.input.allowedsender:

AllowedSender
=============

.. index::
   single: imtcp; AllowedSender
   single: AllowedSender

.. summary-start

Restricts TCP syslog senders by source address or hostname.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: AllowedSender
:Scope: module, input
:Type: array
:Default: module=none, input=module parameter
:Required?: no
:Introduced: 8.2608.0

Description
-----------
Sets source-address ACL entries for TCP syslog inputs. Entries use the same
address syntax as the legacy :doc:`../../configuration/input_directives/rsconf1_allowedsender`
directive, for example ``127.0.0.1/16``, ``192.0.2.0/24``, ``[::1]/128``,
``*.example.net``, or ``somehost.example.com``.

A module-level ``AllowedSender`` list is used as the default for imtcp inputs.
An input-level ``AllowedSender`` list replaces the module-level list for that
input. If neither the module nor the input configures this parameter, all
senders are accepted unless legacy ``$AllowedSender`` directives are present.
If ``AllowedSender`` is configured, the array must contain at least one entry.
An empty input array cannot be used to clear a module-level default. Configure
explicit per-input sender lists, or omit the module-level default if some inputs
must remain unrestricted.
Do not mix legacy ``$AllowedSender`` and modern ``AllowedSender`` in new
configurations.

``AllowedSender`` checks the remote socket address or reverse-DNS hostname.
It is separate from :ref:`param-imtcp-permittedpeer`, which checks TLS peer
identity and only applies to authenticated TLS modes. Firewall rules remain the
preferred first line of defense.

Module usage
------------
.. _param-imtcp-module-allowedsender:
.. _imtcp.parameter.module.allowedsender-usage:

.. code-block:: rsyslog

   module(load="imtcp" allowedSender=["192.0.2.0/24"])

Input usage
-----------
.. _param-imtcp-input-allowedsender:
.. _imtcp.parameter.input.allowedsender-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" allowedSender=["127.0.0.1/16", "[::1]/128"])

YAML usage
----------
.. code-block:: yaml

   modules:
     - load: imtcp
       allowedSender: ["192.0.2.0/24"]
   inputs:
     - type: imtcp
       port: "514"
       allowedSender: ["127.0.0.1/16"]

See also
--------
See also :doc:`../../configuration/modules/imtcp` and
:doc:`../../configuration/input_directives/rsconf1_allowedsender`.
