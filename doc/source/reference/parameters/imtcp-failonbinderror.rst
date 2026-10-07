.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Stop rsyslog startup when an imtcp listener cannot bind.
   :keywords: rsyslog, imtcp, failOnBindError, listener, bind

.. _param-imtcp-failonbinderror:

failOnBindError
===============

.. index::
   single: imtcp; failOnBindError

.. summary-start

Treat a failed TCP listener as a module activation failure.

.. summary-end

:Name: failOnBindError
:Scope: module
:Type: boolean
:Default: off
:Required?: no

When enabled, failure to open any configured ``imtcp`` listener makes module
activation fail. With ``global(abortOnUncleanConfig="on")``, rsyslog then
stops during startup. By default, rsyslog reports the failed port and keeps
working with any listeners that opened successfully.

The bind diagnostic names one visible Linux process holding a listener on the
port when procfs permissions permit it. That process may use a different local
address, and socket ownership may change between the bind attempt and lookup.
If no owner is visible, the diagnostic suggests an ``ss`` command to run with
appropriate permissions.

.. code-block:: rsyslog

   global(abortOnUncleanConfig="on")
   module(load="imtcp" failOnBindError="on")
   input(type="imtcp" address="127.0.0.1" port="6514")

.. code-block:: yaml

   version: 2
   global:
     abortOnUncleanConfig: "on"
   modules:
     - load: imtcp
       failOnBindError: "on"
   inputs:
     - type: imtcp
       address: "127.0.0.1"
       port: "6514"
