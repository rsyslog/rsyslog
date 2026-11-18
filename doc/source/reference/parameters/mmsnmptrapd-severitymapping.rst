.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Set severity mappings for the next legacy mmsnmptrapd action.
   :keywords: rsyslog, mmsnmptrapd, severitymapping, legacy directive

.. _param-mmsnmptrapd-severitymapping:
.. _mmsnmptrapd.parameter.module.severitymapping:

severityMapping
===============

.. index::
   single: mmsnmptrapd; severityMapping
   single: severityMapping

.. summary-start

Defines severity string to numeric code mappings.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmsnmptrapd`.

:Name: severityMapping
:Scope: next legacy action
:Type: string
:Default: action=none
:Required?: no
:Introduced: at least 5.8.1, possibly earlier

Description
-----------
This specifies the severity mapping table. It must be specified as a list.
Note that **no whitespace** is supported inside the list, as it will likely
lead to parsing errors. The list is
constructed of Severity-Name/Severity-Value pairs, delimited by comma.
Severity-Name is a case-sensitive string, e.g. ``warning`` and an associated
numerical value (e.g. 4). Possible values are in the range 0..7 and are defined
in `RFC5424, table 2 <https://datatracker.ietf.org/doc/html/rfc5424#section-6.2.1>`_.
The given sample would be specified as ``warning/4``. The mapping is set
with ``$mmsnmptrapdSeverityMapping`` before the next ``:mmsnmptrapd:`` action.
The setting resets after that action is constructed. Repeat the directive
before another action to give it a different mapping.

Legacy directive usage
----------------------
.. _param-mmsnmptrapd-module-severitymapping:
.. _mmsnmptrapd.parameter.module.severitymapping-usage:

.. rsyslog-doc-sample: validate-config
   :require-plugin: mmsnmptrapd

.. code-block:: rsyslog
   :force:

   module(load="mmsnmptrapd")
   $mmsnmptrapdSeverityMapping warning/4,error/3
   *.* :mmsnmptrapd:

Supported directive
~~~~~~~~~~~~~~~~~~~
This legacy directive is the supported way to configure this setting. It is
not accepted as a ``module()`` or ``action()`` parameter. The setting resets
after the next ``:mmsnmptrapd:`` action is constructed.

.. _mmsnmptrapd.parameter.legacy.mmsnmptrapdseveritymapping:

- ``$mmsnmptrapdSeverityMapping``

.. index::
   single: mmsnmptrapd; $mmsnmptrapdSeverityMapping
   single: $mmsnmptrapdSeverityMapping

See also
--------
See also :doc:`../../configuration/modules/mmsnmptrapd`.
