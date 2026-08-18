.. _param-omfile-dynafile-restricttemplatetype:
.. _omfile.parameter.module.dynafile-restricttemplatetype:

dynafile.restrictTemplateType
=============================

.. meta::
   :description: Optionally restrict omfile dynafile templates to string and list types.
   :keywords: rsyslog, omfile, dynafile, template, path traversal, security

.. index::
   single: omfile; dynafile.restrictTemplateType
   single: dynafile.restrictTemplateType

.. summary-start

Optionally restricts dynafile templates to string and list templates so omfile
can inspect their fixed path prefix before opening rendered paths.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: dynafile.restrictTemplateType
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: not specified

Description
-----------

When enabled, omfile accepts only ``type="string"`` and ``type="list"``
templates for the :ref:`param-omfile-dynafile` parameter. These template
types expose their constant and property entries to omfile, which lets omfile
derive the trusted static directory prefix and block rendered paths that escape
it.

The default preserves plugin/string-generator and subtree dynafile templates.
Those legacy forms use the fallback runtime guard: absolute paths and relative
paths that lexically escape through ``..`` are rejected. Use
``dynafile.dangerousPermitPathEscape="on"`` only when an affected legacy
configuration must retain that behavior. Enable this option when an
installation can use only string or list templates and wants that stricter
configuration-time policy.

Module usage
------------

.. _param-omfile-module-dynafile-restricttemplatetype:
.. _omfile.parameter.module.dynafile-restricttemplatetype-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" dynafile.restrictTemplateType="on")

See also
--------

See also :doc:`../../configuration/modules/omfile` and
:doc:`omfile-dynafile`.
