.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Set the tag prefix for the next legacy mmsnmptrapd action.
   :keywords: rsyslog, mmsnmptrapd, tag, legacy directive

.. _param-mmsnmptrapd-tag:
.. _mmsnmptrapd.parameter.module.tag:

tag
===

.. index::
   single: mmsnmptrapd; tag
   single: tag

.. summary-start

Specifies the tag prefix that identifies messages for processing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmsnmptrapd`.

:Name: tag
:Scope: next legacy action
:Type: word (see :doc:`../../rainerscript/constant_strings`)
:Default: action=snmptrapd
:Required?: no
:Introduced: at least 5.8.1, possibly earlier

Description
-----------
Tells the module which start string inside the tag to look for. The default is
``snmptrapd``. Note that a slash (``/``) is automatically appended to this tag for
matching. You should not include a trailing slash unless you specifically need
to match a double slash. For example, setting ``$mmsnmptrapdTag tag/`` results in a
check for ``tag//`` at the start of the tag field.

Legacy directive usage
----------------------
.. _param-mmsnmptrapd-module-tag:
.. _mmsnmptrapd.parameter.module.tag-usage:

.. rsyslog-doc-sample: validate-config
   :require-plugin: mmsnmptrapd

.. code-block:: rsyslog

   module(load="mmsnmptrapd")
   $mmsnmptrapdTag snmptrapd
   *.* :mmsnmptrapd:

Supported directive
~~~~~~~~~~~~~~~~~~~
This legacy directive is the supported way to configure this setting. It is
not accepted as a ``module()`` or ``action()`` parameter. The setting resets
after the next ``:mmsnmptrapd:`` action is constructed.

.. _mmsnmptrapd.parameter.legacy.mmsnmptrapdtag:

- ``$mmsnmptrapdTag``

.. index::
   single: mmsnmptrapd; $mmsnmptrapdTag
   single: $mmsnmptrapdTag

See also
--------
See also :doc:`../../configuration/modules/mmsnmptrapd`.
