.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Make imptcp bind failures fatal when abortOnUncleanConfig is enabled.
   :keywords: rsyslog, imptcp, failOnBindError, listener, bind

.. _param-imptcp-failonbinderror:

failOnBindError
===============

.. index::
   single: imptcp; failOnBindError

.. summary-start

Treat a failed TCP listener as a module activation failure.

.. summary-end

:Name: failOnBindError
:Scope: module
:Type: boolean
:Default: off
:Required?: no

When enabled, a TCP bind failure makes module activation fail even if another
``imptcp`` listener opened successfully. With
``global(abortOnUncleanConfig="on")``, rsyslog then stops during startup. By
default, rsyslog reports the failed port and keeps working with any listeners
that opened successfully. If no listener opens, module activation fails even
with this option disabled.

For address-in-use bind failures, the diagnostic names one visible Linux process
holding a listener on the port when procfs permissions permit it. That process
may use a different local address, and socket ownership may change between the
bind attempt and lookup. If no owner is visible on Linux, the diagnostic
suggests an ``ss`` command to run with appropriate permissions.

.. code-block:: rsyslog

   global(abortOnUncleanConfig="on")
   module(load="imptcp" failOnBindError="on")
   input(type="imptcp" address="127.0.0.1" port="6514")

.. code-block:: yaml

   version: 2
   global:
     abortOnUncleanConfig: "on"
   modules:
     - load: imptcp
       failOnBindError: "on"
   inputs:
     - type: imptcp
       address: "127.0.0.1"
       port: "6514"
