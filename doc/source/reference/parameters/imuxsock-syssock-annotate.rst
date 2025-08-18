.. _param-imuxsock-syssock-annotate:
.. _imuxsock.parameter.module.syssock-annotate:

SysSock.Annotate
================

.. index::
   single: imuxsock; SysSock.Annotate
   single: SysSock.Annotate

.. summary-start

Enables annotation or trusted properties on the system log socket.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.Annotate
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Turn on annotation/trusted properties for the system log socket. See
the :ref:`imuxsock-trusted-properties-label` section for more info.

Module usage
------------
.. _param-imuxsock-module-syssock-annotate:
.. _imuxsock.parameter.module.syssock-annotate-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.annotate="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.systemlogsockannotate:

- $SystemLogSocketAnnotate — maps to SysSock.Annotate (status: legacy)

.. index::
   single: imuxsock; $SystemLogSocketAnnotate
   single: $SystemLogSocketAnnotate

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
