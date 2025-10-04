.. _param-imdtls-name:
.. _imdtls.parameter.input.name:

Name
====

.. index::
   single: imdtls; name
   single: name

.. summary-start


Assigns a unique identifier to the imdtls input instance.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: name
:Scope: input
:Type: word
:Default: none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Provides a unique name to the input module instance. This is useful for
identifying the source of messages when multiple input modules are used.

Input usage
-----------
.. _param-imdtls-input-name:
.. _imdtls.parameter.input.name-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls" name="dtlsListener01")

See also
--------
See also :doc:`../../configuration/modules/imdtls`.
