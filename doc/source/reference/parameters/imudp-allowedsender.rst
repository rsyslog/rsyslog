.. _param-imudp-allowedsender:
.. _imudp.parameter.module.allowedsender:
.. _imudp.parameter.input.allowedsender:

AllowedSender
=============

.. index::
   single: imudp; AllowedSender
   single: AllowedSender

.. summary-start

Restricts UDP syslog senders by source address or hostname.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: AllowedSender
:Scope: module, input
:Type: array
:Default: module=none, input=module parameter
:Required?: no
:Introduced: 8.2608.0

Description
-----------
Sets source-address ACL entries for UDP syslog inputs. Entries use the same
address syntax as the legacy :doc:`../../configuration/input_directives/rsconf1_allowedsender`
directive, for example ``127.0.0.1/16``, ``192.0.2.0/24``, ``[::1]/128``,
``*.example.net``, or ``somehost.example.com``.

A module-level ``AllowedSender`` list is used as the default for imudp inputs.
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
By UDP design, source addresses can be spoofed. Use firewall ingress and egress
filtering, and prefer TCP or TLS transports when sender authenticity matters.

Module usage
------------
.. _param-imudp-module-allowedsender:
.. _imudp.parameter.module.allowedsender-usage:

.. code-block:: rsyslog

   module(load="imudp" allowedSender=["192.0.2.0/24"])

Input usage
-----------
.. _param-imudp-input-allowedsender:
.. _imudp.parameter.input.allowedsender-usage:

.. code-block:: rsyslog

   input(type="imudp" port="514" allowedSender=["127.0.0.1/16"])

YAML usage
----------
.. code-block:: yaml

   modules:
     - load: imudp
       allowedSender: ["192.0.2.0/24"]
   inputs:
     - type: imudp
       port: "514"
       allowedSender: ["127.0.0.1/16"]

See also
--------
See also :doc:`../../configuration/modules/imudp` and
:doc:`../../configuration/input_directives/rsconf1_allowedsender`.
