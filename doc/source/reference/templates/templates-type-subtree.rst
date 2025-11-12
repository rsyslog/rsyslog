.. _ref-templates-type-subtree:
.. _templates.parameter.type-subtree:

Subtree template type
=====================

.. index::
   single: template; type=subtree
   single: templates; subtree type

.. summary-start

Builds output from a full JSON subtree (CEE).
Best used when the schema has already been remapped and an appropriate variable tree exists.

.. summary-end

:Name: ``type="subtree"``
:Scope: template
:Type: subtree
:Introduced: 7.1.4

Description
-----------

The *subtree template type* generates output from a complete (CEE/JSON) subtree.
This is useful when working with **data pipelines** where schema mapping
is done beforehand and a full variable tree (e.g. ``$!ecs``) is available.

This method is required when an entire subtree must be placed at the root of
the generated object. With other template types, only sub-containers can be
produced. Constant text cannot be inserted inside subtree templates.

Subtree templates are often used with structured outputs such as
:ref:`ommongodb <ref-ommongodb>`, :ref:`omelasticsearch <ref-omelasticsearch>`,
or with text-based outputs like :ref:`omfile <ref-omfile>`.

They are particularly effective after message transformation with parsing
modules such as :ref:`mmjsonparse <ref-mmjsonparse>`,
:ref:`mmaudit <ref-mmaudit>`, or :ref:`mmleefparse <ref-mmleefparse>`.

Example: ECS mapping
--------------------

A typical workflow is to normalize message content into an ECS-compatible
subtree and then export it with a subtree template:

.. code-block:: none

   set $!ecs!event!original = $msg;
   set $!ecs!host!hostname = $hostname;
   set $!ecs!log!level = $syslogseverity-text;
   set $!ecs!observer!type = "rsyslog";
   template(name="ecs_tpl" type="subtree" subtree="$!ecs")

Here the message is mapped into ECS fields under ``$!ecs``. The complete
ECS subtree is then emitted as JSON by the template.

Data pipeline usage
-------------------

Subtree templates are a natural part of rsyslog data pipelines:

.. mermaid::

   flowchart TD
      A["Input<br>(imudp, imtcp, imkafka)"]
      B["Parser<br>(mmjsonparse, mmaudit, ...)"]
      C["Schema Tree<br>($!ecs)"]
      D["Template<br>type=subtree"]
      E["Action<br>(omfile, omkafka, ...)"]

      A --> B --> C --> D --> E

Alternative mapping approach
----------------------------

If you do **not** yet have a remapped schema tree,
consider using a :ref:`list template <ref-templates-type-list>` instead.
List templates allow mapping fields one-by-one into structured output
before exporting.

Notes
-----

* Use subtree templates when a full schema tree is already present.
* Use list templates when building or remapping the schema incrementally.

See also
--------

* :ref:`ref-templates-type-list`
* :ref:`ref-templates`
* :ref:`ref-ommongodb`
* :ref:`ref-omelasticsearch`
* :ref:`ref-omfile`
* :ref:`ref-omkafka`
