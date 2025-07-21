Input
=====

.. index:: ! input
.. _cfgobj_input:

The ``input`` object, as its name suggests, describes message input sources.
Without input, no processing happens at all, because no messages enter the
rsyslog system.
Inputs are implemented via :doc:`input modules <modules/idx_input>`.

The input object has different parameters:

-  those that apply to all input and are generally available for
   all inputs. These are documented below.
-  input-specific parameters. These are specific to a certain type of
   input. They are documented by the :doc:`input module <modules/idx_input>`
   in question.

General Input Parameters
------------------------

Note: parameter names are case-insensitive.

.. function::  type <type-string>

   *Mandatory*

   The ``<type-string>`` is a string identifying the input module as given
   it each module's documentation. For example, the
   :doc:`UDP syslog input <modules/imudp>` is named "imudp".

