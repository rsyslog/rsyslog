.. _param-omfile-ziplevel:
.. _omfile.parameter.module.ziplevel:

zipLevel
========

.. index::
   single: omfile; zipLevel
   single: zipLevel

.. summary-start

If greater than 0, turns on gzip compression of the output file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: zipLevel
:Scope: action
:Type: integer
:Default: action=0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

If greater than 0, turns on gzip compression of the output file. The
higher the number, the better the compression, but also the more CPU
is required for zipping.

Action usage
------------

.. _param-omfile-action-ziplevel:
.. _omfile.parameter.action.ziplevel:
.. code-block:: rsyslog

   action(type="omfile" zipLevel="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.omfileziplevel:

- $OMFileZipLevel — maps to zipLevel (status: legacy)

.. index::
   single: omfile; $OMFileZipLevel
   single: $OMFileZipLevel

See also
--------

See also :doc:`../../configuration/modules/omfile`.
