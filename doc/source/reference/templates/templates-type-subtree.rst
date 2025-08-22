.. _ref-templates-type-subtree:

Subtree template type
=====================

.. summary-start

Builds output from an entire CEE subtree.
Useful for hierarchical data where the structure is prepared beforehand.
.. summary-end

Available since rsyslog 7.1.4

The template is generated based on a complete (CEE) subtree. This type is
most useful for outputs that understand hierarchical structures, such as
ommongodb. The parameter ``subtree`` selects which subtree to include.
For example ``template(name="tpl1" type="subtree" subtree="$!")``
includes all CEE data, while ``template(name="tpl2" type="subtree" subtree="$!usr!tpl2")``
includes only the subtree starting at ``$!usr!tpl2``.

This method must be used if a complete subtree needs to be placed directly
into the object's root. With other template types, only subcontainers can
be generated. Subtree templates can be used with text-based outputs such as
omfile, but constant text like line breaks cannot be inserted.

Use case
--------

A common approach is to create a custom subtree and then include it in the
template:

.. code-block:: none

   set $!usr!tpl2!msg = $msg;
   set $!usr!tpl2!dataflow = field($msg, 58, 2);
   template(name="tpl2" type="subtree" subtree="$!usr!tpl2")

Here ``$msg`` is assumed to contain several fields, one of which is
extracted and stored—together with the message—as field content.

