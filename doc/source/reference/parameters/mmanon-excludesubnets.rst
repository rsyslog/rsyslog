.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Preserve selected IPv4 and IPv6 address literals when mmanon anonymizes messages.
   :keywords: rsyslog, mmanon, excludeSubnets, CIDR, IPv4, IPv6

.. _param-mmanon-excludesubnets:
.. _mmanon.parameter.input.excludesubnets:

excludeSubnets
==============

.. index::
   single: mmanon; excludeSubnets
   single: excludeSubnets

.. summary-start

Lists IPv4 and IPv6 CIDR subnets whose matching address literals are left unchanged.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: excludeSubnets
:Scope: input
:Type: array of IPv4 or IPv6 CIDR strings
:Default: input=empty
:Required?: no
:Introduced: 8.2612.0

Description
-----------
When this array is empty or omitted, mmanon anonymizes addresses as before.
Each entry must be an IPv4 or IPv6 address followed by a slash and a decimal
prefix length, such as ``192.0.2.0/24`` or ``2001:db8:1::/48``. Invalid entries,
including prefixes longer than 32 for IPv4 or 128 for IPv6, make configuration
validation fail.

An address literal is excluded when its address family matches an entry and
its prefix bits match. Thus ``192.0.2.0/24`` includes ``192.0.2.0`` through
``192.0.2.255`` but not ``192.0.3.0``. For an IPv6 address written with a
dotted IPv4 tail, the full literal is preserved if either the IPv6 address
matches an IPv6 entry or the dotted tail matches an IPv4 entry. Excluded
literals are skipped by all mmanon address scanners, so the embedded tail is
not anonymized later in the same action.

Input usage
-----------
RainerScript:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" excludeSubnets=["192.0.2.0/24", "2001:db8:1::/48"])

YAML:

.. code-block:: yaml

   modules:
     - load: mmanon
   rulesets:
     - name: main
       actions:
         - type: mmanon
           excludeSubnets:
             - "192.0.2.0/24"
             - "2001:db8:1::/48"

See also
--------
:doc:`../../configuration/modules/mmanon`
