.. _tutorial-config-format-translation:

Translating Between RainerScript and YAML
=========================================

.. meta::
   :description: Tutorial for translating rsyslog configurations between RainerScript and YAML with rsyslogd validation mode.
   :keywords: rsyslog, translation, rainerscript, yaml, config migration, -N1, -F

.. summary-start

rsyslog can translate configurations between canonical RainerScript and YAML.
Use this to migrate existing configs, review equivalent syntax, and standardize
mixed estates without changing the effective configuration model.

.. summary-end

This tutorial focuses on the most common migration path: translating an
existing RainerScript configuration to YAML. Translation runs as part of
config validation, so rsyslog reads the effective configuration and writes a
new output file without starting the daemon.

What Translation Does and Does Not Do
-------------------------------------

The translator is a semantic migration tool:

- It preserves effective configuration behavior for supported constructs.
- It writes canonical output in the requested target format.
- It flattens include trees into one generated file.
- It may emit warning comments for constructs that are easy to misunderstand,
  especially legacy syntax.
- It can normalize a limited set of common legacy selector/action lines into
  structured YAML instead of falling back to raw RainerScript text.

It is not a source-preserving formatter:

- Comments, whitespace, and include boundaries are not preserved.
- Output is normalized into one canonical file.
- The generated YAML or RainerScript may look different from the original even
  when the behavior is unchanged.
- Legacy coverage is intentionally limited. Common file actions
  (``/var/log/...`` and ``-/var/log/...``) and ``:omusrmsg:...`` are
  recognized, but many other legacy forms still fall back to warnings or
  ``script: |`` output.

Translate RainerScript to YAML
------------------------------

Assume this RainerScript file:

.. code-block:: rsyslog

   main_queue(queue.type="Direct")
   ruleset(name="main") {
     action(type="omfile" file="/var/log/sample.log")
   }

Translate it with:

.. code-block:: shell

   rsyslogd -N1 -f /path/to/input.conf -F yaml -o /path/to/output.yaml

The important flags are:

- ``-N1``: read and validate the configuration without starting rsyslog
- ``-f``: input config
- ``-F yaml``: requested output format
- ``-o``: generated output file

A simple action-only ruleset is translated into structured YAML:

.. code-block:: yaml

   version: 2

   mainqueue:
     queue.type: "Direct"

   rulesets:
     - name: "main"
       actions:
         - type: "omfile"
           file: "/var/log/sample.log"

Review Translation Warnings
---------------------------

If rsyslog encounters constructs that may surprise a reader after translation,
it emits warning comments directly into the generated output. Typical cases are:

- legacy ``$`` directives
- traditional selector/action syntax
- normalization of inherited defaults into explicit settings
- target-format fallbacks such as YAML ``script: |`` blocks for complex ruleset bodies

The warning prefix is:

.. code-block:: text

   # TRANSLATION WARNING: ...

Always review these comments before adopting the generated file.

Validate the Result
-------------------

After translation, validate the generated file just like any other config:

.. code-block:: shell

   rsyslogd -N1 -f /path/to/output.yaml

This is especially useful after reviewing or editing the generated output.

For simple rulesets, translation emits structured YAML such as ``actions:`` or
``filter:`` + ``actions:``. More complex logic still falls back to a YAML
``script: |`` block so semantics remain explicit.

Limited Legacy Translation
--------------------------

Common distro defaults written in traditional selector/action syntax can now be
translated into native YAML structures.

For example, this legacy input:

.. code-block:: rsyslog

   user.*                -/var/log/user.log
   *.emerg               :omusrmsg:*

is translated into YAML like:

.. code-block:: yaml

   version: 2

   rulesets:
     - name: "RSYSLOG_DefaultRuleset"
       statements:
         - if: "prifilt('user.*')"
           action:
             type: "omfile"
             file: "/var/log/user.log"
         - if: "prifilt('*.emerg')"
           action:
             type: "omusrmsg"
             users: "*"

Current legacy coverage is intentionally narrow:

- file actions written as ``/path`` or ``-/path`` become ``omfile`` actions
- ``:omusrmsg:user`` becomes ``omusrmsg`` with ``users: ...``
- selector chains are normalized into YAML ``statements:``

Do not expect all legacy constructs to be converted into structured YAML.
Unsupported or ambiguous cases may still produce warning comments or fall back
to ``script: |``.

Translate YAML to RainerScript
------------------------------

The reverse direction uses the same workflow:

.. code-block:: shell

   rsyslogd -N1 -f /path/to/input.yaml -F rainerscript -o /path/to/output.conf

This is helpful when you want to:

- inspect YAML configs in canonical RainerScript form
- integrate YAML-authored config into RainerScript-oriented workflows
- compare equivalent forms during migration reviews

Working with Includes
---------------------

Translation reads the full effective configuration, including included files,
and writes one canonical output file. That means:

- include boundaries are flattened
- the generated file shows the combined configuration as rsyslog sees it
- the result is suited for migration and review, not for preserving source
  layout

If you need original file structure, keep the source tree alongside the
translated output.

Practical Migration Workflow
----------------------------

For an existing RainerScript deployment, this workflow is usually sufficient:

1. Translate the current main config to YAML.
2. Review the generated file and any warning comments.
3. Validate the generated YAML with ``-N1``.
4. Test it in your normal deployment workflow.
5. Keep the generated file as the new canonical config if it fits your
   environment better.

Related Documentation
---------------------

- :doc:`../configuration/conf_formats`
- :doc:`../configuration/yaml_config`
- :doc:`../configuration/converting_to_new_format`
- :doc:`../troubleshooting/troubleshoot`
