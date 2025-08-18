.. _param-imuxsock-parsetrusted:
.. _imuxsock.parameter.input.parsetrusted:

ParseTrusted
============

.. index::
   single: imuxsock; ParseTrusted
   single: ParseTrusted

.. summary-start

Turns annotation data into JSON fields when annotation is enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: ParseTrusted
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Equivalent to the ``SysSock.ParseTrusted`` module parameter, but applies
to the input that is being defined.

Input usage
-----------
.. _param-imuxsock-input-parsetrusted:
.. _imuxsock.parameter.input.parsetrusted-usage:

.. code-block:: rsyslog

   input(type="imuxsock" parseTrusted="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.parsetrusted:

- $ParseTrusted — maps to ParseTrusted (status: legacy)

.. index::
   single: imuxsock; $ParseTrusted
   single: $ParseTrusted

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
