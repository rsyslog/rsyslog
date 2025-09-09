Documentation Reference Section Structure Guidelines
====================================================

This page describes the desired layout for reference documentation pages.
It reflects the new per-parameter reference model used in the rsyslog
documentation archive.

Overview
--------

Each module (for example ``omfwd``) keeps its master RST file under
``source/configuration/modules/``.  Parameters are moved into individual
files under ``source/reference/parameters/``.  The master page lists all
parameters in compact tables and links to the per-parameter files.

**Do not move or rename existing module files**.  Parameter files may be
added and removed freely.

Directory Layout
----------------

Modules stay at the top level while parameter files live in the shared
``reference/parameters`` directory.

.. code-block:: text

   source/
   ├── configuration/modules/
   │   ├── omfwd.rst                # master reference page (unchanged)
   │   └── imfile.rst               # other modules (unchanged)
   └── reference/parameters/
       ├── omfwd-target.rst
       ├── omfwd-port.rst
       └── imfile-seekable.rst

Module Page (``<module>.rst``)
------------------------------

Enhance the existing module page with:

#. Narrative content (purpose, examples, performance notes, etc.).
#. **Module Parameters** and **Input Parameters** tables. Use the Module table
   for parameters configured through ``module()`` statements or global module
   settings. Use the Input table for parameters set in per-instance
   ``input()`` or ``action()`` blocks.
#. A note about case-insensitive parameter names.
#. A hidden ``.. toctree::`` pulling in all parameter files for this module.

**Example parameters section**

.. code-block:: rst

   .. note::

      Parameter names are case-insensitive; camelCase is recommended
      for readability.

   **Module Parameters**

   .. list-table::
      :widths: 30 70
      :header-rows: 1

      * - Parameter
        - Summary
      * - :ref:`param-omfwd-target`
        - .. include:: ../../reference/parameters/omfwd-target.rst
           :start-after: .. summary-start
           :end-before: .. summary-end

   **Input Parameters**

   .. list-table::
      :widths: 30 70
      :header-rows: 1

      * - Parameter
        - Summary
      * - :ref:`param-omfwd-port`
        - .. include:: ../../reference/parameters/omfwd-port.rst
           :start-after: .. summary-start
           :end-before: .. summary-end

   .. toctree::
      :hidden:

      ../../reference/parameters/omfwd-target
      ../../reference/parameters/omfwd-port

Parameter Files (``reference/parameters/<module>-<param>.rst``)
---------------------------------------------------------------

Create one file per parameter using the pattern
``<module>-<param-lower>.rst``.  Use lowercase in filenames and anchor
leaf names; replace dots with hyphens.

Each file must contain:

* Canonical anchor: ``.. _param-<module>-<param-lower>:``
* Scoped anchor: ``.. _<module>.parameter.<scope>.<param-lower>:``
  where ``<scope>`` is ``module`` or ``input``. Use ``module`` for parameters
  set in ``module()`` statements or affecting the module globally. Use
  ``input`` for per-instance parameters inside ``input()`` or ``action()``
  blocks.
* Section heading matching the parameter name
* ``.. summary-start`` and ``.. summary-end`` markers
* Description copied from the original module page
* A usage subsection titled ``Module usage`` or ``Input usage`` (matching the
  scope) with a usage example and a matching ``-usage`` anchor
* Optional ``Legacy names`` subsection for deprecated directives
* ``See also`` section referencing the module page

Anchors & Cross-References
---------------------------

* Reference parameters from module pages with::

      :ref:`param-<module>-<param-lower>`
* Usage anchors must end in ``-usage`` and appear next to the example
  ``code-block``.
* Do not duplicate anchors across files.

Rules for Examples
------------------

* Parameter names are case-insensitive.  Examples should use
  ``camelCase`` unless a module specifically requires ``CamelCase``.
* Headings and canonical parameter names retain their original casing.

Validation
----------

* All parameter files referenced in a module must exist.
* Sphinx must build without duplicate label warnings or unresolved
  references.
* Every parameter from the original module page appears in the new tables.

TOC Integration
---------------

**In the module index** (e.g., ``source/configuration/modules/index.rst``):

Ensure that all module pages are listed in the appropriate index file's toctree to make them discoverable.

.. code-block:: rst

   .. toctree::
      :maxdepth: 2

      omfwd
      omrelp
      imfile
      # ... and so on

Add the following to ``doc_style_guide.rst``::

   .. toctree::
      :maxdepth: 1

      doc_style_guide
      reference_section_guidelines
