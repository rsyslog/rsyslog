.. _param-imdtls-ruleset:
.. _imdtls.parameter.input.ruleset:

Ruleset
=======

.. index::
   single: imdtls; Ruleset
   single: Ruleset

.. summary-start


Binds received DTLS messages to the specified processing ruleset.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: Ruleset
:Scope: input
:Type: word
:Default: none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Determines the ruleset to which the imdtls input will be bound. This can be overridden at the instance level.

Input usage
-----------
.. _param-imdtls-input-ruleset:
.. _imdtls.parameter.input.ruleset-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls" ruleset="secure-logs")

See also
--------
See also :doc:`../../configuration/modules/imdtls`.
