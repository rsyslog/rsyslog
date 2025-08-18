.. _param-imuxsock-annotate:
.. _imuxsock.parameter.input.annotate:

Annotate
========

.. index::
   single: imuxsock; Annotate
   single: Annotate

.. summary-start

Enables annotation and trusted properties for this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: Annotate
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Turn on annotation/trusted properties for the input that is being defined.
See the :ref:`imuxsock-trusted-properties-label` section for more info.

Input usage
-----------
.. _param-imuxsock-input-annotate:
.. _imuxsock.parameter.input.annotate-usage:

.. code-block:: rsyslog

   input(type="imuxsock" annotate="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imuxsock.parameter.legacy.inputunixlistensocketannotate:
- $InputUnixListenSocketAnnotate — maps to Annotate (status: legacy)

.. index::
   single: imuxsock; $InputUnixListenSocketAnnotate
   single: $InputUnixListenSocketAnnotate

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
