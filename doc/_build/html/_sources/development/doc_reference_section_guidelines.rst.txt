Documentation Reference Section Structure Guidelines
====================================================

This page describes the **ultimate desired layout** for all _reference_ pages
(e.g. modules under Configuration → Output Modules), balancing both:

- **Human UX** under the Furo theme  
- **AI/RAG ingestion** needs for fine-grained, semantically focused chunks

Overview
--------

Each reference module (e.g., ``omfwd``) is defined by its master RST file and
an optional set of category pages for related parameters. **Do not move or rename
existing module files** (e.g., ``source/configuration/modules/omfwd.rst``) to
preserve SEO, bookmarks, and inter-document links.

Directory Layout
----------------

Modules remain at top level; parameter group pages live in a new sub-folder.

.. code-block:: text

   source/configuration/modules/
   ├── omfwd.rst                    # master reference page (unchanged)
   ├── imfile.rst                   # other modules (unchanged)
   └── omfwd/                       # new subdirectory for parameter categories
       └── parameters/
           ├── basic.rst            # core parameters (target, port, protocol)
           ├── transport.rst        # networking params (Address, NetworkNamespace)
           ├── security.rst         # TLS/auth params (StreamDriver, AuthMode)
           └── advanced.rst         # compression, framing, keep-alive, etc.

Master Page ("<module>.rst")
----------------------------

Enhance the existing master file (e.g., ``omfwd.rst``) with these sections:

#. **Introduction** and **Best Practices** (concise overview).  
#. **Common Pitfalls** of legacy syntax with minimal before/after snippets.  
#. **Parameters at a Glance** — two tables:
   - **Module-Load Parameters** (e.g., ``iobuffer.maxSize``)  
   - **Action-Instance Parameters** (e.g., ``target``, ``queue.type``)  
#. A hidden ``.. toctree::`` pulling in the category pages:

   .. code-block:: rst

      .. toctree::
         :hidden:
         :maxdepth: 1

         omfwd/parameters/basic
         omfwd/parameters/transport
         omfwd/parameters/security
         omfwd/parameters/advanced

**Example Parameters at a Glance:**

.. code-block:: rst

   **Module-Load Parameters**

   .. csv-table::
      :header: "Parameter", "Default", "Description", "Page"
      :widths: 20 15 50 25

      "iobuffer.maxSize", "full size", "Max TCP API buffer.", :doc:`module <omfwd/parameters/module>`

   **Action-Instance Parameters**

   .. csv-table::
      :header: "Parameter", "Default", "Description", "Page"
      :widths: 20 15 50 25

      "target", "none", "Remote host(s).", :doc:`action <omfwd/parameters/action>`
      "port", "514", "Destination port.", :doc:`action <omfwd/parameters/action>`
      "protocol", "udp", "Transport protocol (udp/tcp/TLS).", :doc:`action <omfwd/parameters/action>`
      "queue.type", "linkedList", "Per-action queue type.", :doc:`action <omfwd/parameters/action>`

Category Pages ("parameters/\*.rst")
------------------------------------

Group related parameters into 3–8 per category. Example structure for "basic.rst":

.. code-block:: rst

   Basic Parameters
   ================

   .. meta::
      :tag: module:omfwd
      :tag: category:basic

   Core settings for forwarding.

   ---

   target
   ~~~~~~

   **Summary:** Name or IP of the remote syslog server.

   :type: array/word  
   :default: none  
   :mandatory: no  

   .. code-block:: rsyslog

      action(
        type="omfwd"
        target="logs.example.com"
        port="514"
        protocol="tcp"
        queue.type="linkedList"
      )

   .. toggle::
      :caption: Deep dive: How target pools work

      .. include:: ../../../concepts/queues.inc

Repeat for transport, security, and advanced categories.

Concept Snippets
----------------

Centralize deep theory under ``source/concepts/``:

- ``<topic>.inc`` — conceptual snippet content  
- ``<topic>.rst`` — standalone concept page including the snippet

Include snippets in parameter pages via:

.. code-block:: rst

   .. toggle::
      :caption: Deep dive: How target pools work

      .. include:: ../../../concepts/queues.inc

Metadata & Tags
---------------

At the top of every page (master, category, concept), include:

.. code-block:: rst

   .. meta::
      :tag: module:<module-name>
      :tag: category:<category-name>    # category pages only
      :tag: parameter:<parameter-name>  # if desired on parameter sections
      :tag: concept:<topic-name>        # on concept pages

These tags drive precise AI/RAG filtering.

TOC Integration
---------------

**In Doc Writing → Development** (``source/development/doc_style_guide.rst``):

.. code-block:: rst

   .. toctree::
      :maxdepth: 1

      doc_style_guide
      reference_section_guidelines

**In Configuration → Output Modules** (``source/configuration/modules/index.rst``):

.. code-block:: rst

   .. toctree::
      :maxdepth: 2

      omfwd
      omrelp
      omfile
      …

