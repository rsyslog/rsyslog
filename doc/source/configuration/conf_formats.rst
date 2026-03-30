.. _conf_formats:

Configuration Formats
=====================

.. meta::
   :description: Overview of rsyslog configuration formats: basic (sysklogd), advanced (RainerScript), YAML, and the obsolete legacy format.
   :keywords: configuration format, rainerscript, yaml, sysklogd, legacy, conf_formats

.. summary-start

rsyslog supports four configuration formats. For modern configurations, choose
RainerScript or YAML based on workflow fit: RainerScript for direct rsyslog
authoring, YAML for YAML-centric environments and automation.

.. summary-end

Rsyslog has evolved over several decades. For this reason, it supports four
different configuration formats:

-  |FmtBasicName| - previously known as the :doc:`sysklogd  <sysklogd_format>`
   format. Best for simple, one-line configurations matching on facility/severity
   and writing to a log file.

-  |FmtAdvancedName| - previously known as the ``RainerScript`` format. The
   recommended format when you author rsyslog logic directly. First available
   in rsyslog v6, it handles advanced filtering, forwarding, queueing, and
   custom actions.

-  |FmtYamlName| - an additional syntax for YAML-centric environments such as
   Kubernetes, Ansible, GitOps workflows, generated config, or other tooling
   that already speaks YAML. Every YAML key maps directly to the equivalent
   RainerScript parameter, so both formats share the same feature set and core
   rsyslog model. See :doc:`yaml_config` for full reference.

-  |FmtObsoleteName| - previously known as the ``legacy`` format. This format is
   **obsolete** and must not be used in new configurations.

Which Format Should I Use?
--------------------------

For Modern Configurations
~~~~~~~~~~~~~~~~~~~~~~~~~

Choose between |FmtAdvancedName| and |FmtYamlName| based on environment fit,
not ideology:

- Use |FmtAdvancedName| when your team already works effectively with
  RainerScript, when you author rsyslog logic directly, or when that is the
  clearer fit for your workflow.
- Use |FmtYamlName| when rsyslog is part of a broader YAML-centric deployment
  or automation workflow and you want to avoid conversion layers or embedded
  RainerScript text.

The two formats are functionally equivalent for the base configuration model.
You can also mix them: a YAML main config may include RainerScript ``.conf``
fragments and vice versa.

Existing Configurations in Basic Format
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Some distributions ship default configurations in |FmtBasicName| format.
Converting them to |FmtAdvancedName| is straightforward and recommended
if you plan to add complex constructs such as rulesets or queued actions.

Retaining Basic Format
~~~~~~~~~~~~~~~~~~~~~~

Continue using the |FmtBasicName| format if there is a strong reliance on
external documentation or many existing configurations in that format.

Example - Basic Format
^^^^^^^^^^^^^^^^^^^^^^

::

    mail.info /var/log/mail.log
    mail.err @@server.example.net

Advanced Use Cases
~~~~~~~~~~~~~~~~~~

For direct authoring beyond basic logging, |FmtAdvancedName| remains an
excellent fit:

- Fine control via advanced parameters
- Easy-to-follow block structure
- Safe for use with include files

Example - Advanced Format
^^^^^^^^^^^^^^^^^^^^^^^^^

::

    mail.err action(type="omfwd" protocol="tcp" queue.type="linkedList")

Deprecated Format
~~~~~~~~~~~~~~~~~

Do not use |FmtObsoleteName| format. **It is obsolete and will make your
life difficult.** It exists solely for backward compatibility with very
old configurations.

Conclusion
----------

Use |FmtAdvancedName| for new configurations, or the :doc:`YAML format
<yaml_config>` when rsyslog lives inside a YAML-centric workflow. The
|FmtBasicName| format is acceptable for simple, existing configurations.
Never use |FmtObsoleteName|.
