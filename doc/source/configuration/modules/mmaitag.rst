.. index:: ! mmaitag

*********************************
AI-based classification (mmaitag)
*********************************

================  ================================
**Module Name:**  mmaitag
**Author:**:      Adiscon
**Available:**:   9.0+
================  ================================

Purpose
=======

The **mmaitag** module enriches log messages with classification tags
obtained from an external AI service. Each message is sent to the provider
individually and the resulting tag is stored in a custom variable.

Default labels
--------------

| Label     | Description                                                |
| --------- | ---------------------------------------------------------- |
| NOISE     | Can be ignored, redundant, or irrelevant for most purposes |
| REGULAR   | Normal messages of operational interest                    |
| IMPORTANT | Should be logged and may indicate early signs of issues    |
| CRITICAL  | Indicates immediate or serious problems                    |

Action Parameters
=================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for
   readability.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmaitag-provider`
     - .. include:: ../../reference/parameters/mmaitag-provider.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmaitag-tag`
     - .. include:: ../../reference/parameters/mmaitag-tag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmaitag-model`
     - .. include:: ../../reference/parameters/mmaitag-model.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmaitag-expert-initial_prompt`
     - .. include:: ../../reference/parameters/mmaitag-expert-initial_prompt.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmaitag-inputproperty`
     - .. include:: ../../reference/parameters/mmaitag-inputproperty.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmaitag-apikey`
     - .. include:: ../../reference/parameters/mmaitag-apikey.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmaitag-apikey_file`
     - .. include:: ../../reference/parameters/mmaitag-apikey_file.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/mmaitag-provider
   ../../reference/parameters/mmaitag-tag
   ../../reference/parameters/mmaitag-model
   ../../reference/parameters/mmaitag-expert-initial_prompt
   ../../reference/parameters/mmaitag-inputproperty
   ../../reference/parameters/mmaitag-apikey
   ../../reference/parameters/mmaitag-apikey_file

Example
=======

.. code-block:: rsyslog

    module(load="mmaitag")
    action(type="mmaitag" provider="gemini" apikey="ABC" tag="$.aitag")
