.. _log-pipeline-example-json:

======================================
Example: JSON Parse and Transformation
======================================

.. meta::
   :description: Example rsyslog configuration showing JSON parsing and unflatten transformation in the log pipeline.
   :keywords: rsyslog, log pipeline, mmjsonparse, mmjsontransform, unflatten, json, structured logging

.. summary-start

Demonstrates a complete mini log pipeline that parses JSON logs, converts dotted
keys into nested objects with ``unflatten``, and writes a clean structured subtree.

.. summary-end


Overview
--------
This example illustrates how the **log pipeline** concept applies in a real
configuration.  It transforms raw QRadar-style JSON messages with dotted keys
(e.g., ``src.ip``) into fully structured nested JSON.  
The workflow shows the connection between **input**, **ruleset logic**, and
**output actions**, using ``mmjsonparse`` and ``mmjsontransform`` modules.

.. mermaid::

   flowchart LR
     I["Input<br>(raw JSON)"]:::input --> P["mmjsonparse<br>parse JSON"]:::ruleset --> T["mmjsontransform<br>unflatten keys"]:::ruleset --> O["omfile<br>structured output"]:::action

     classDef input fill:#d5e8d4,stroke:#82b366;
     classDef ruleset fill:#dae8fc,stroke:#6c8ebf;
     classDef action fill:#ffe6cc,stroke:#d79b00;

Configuration
-------------
The configuration below defines a small self-contained pipeline that reads JSON
from the raw payload, builds a nested subtree, and writes it to a file.

.. code-block:: rsyslog
   :caption: JSON → transform → file (commented)

   # Template: output only the structured subtree we build
   template(name="outfmt" type="subtree" subtree="$!qradar_structured")

   # Load parser and transformer modules
   module(load="mmjsonparse")
   module(load="mmjsontransform")

   # Parse JSON directly from the raw message
   action(
     type="mmjsonparse"
     container="$!qradar"
     mode="find-json"
     useRawMsg="on"
   )

   # Proceed only if parsing succeeded
   if $parsesuccess == "OK" then {
     # Convert dotted keys into nested JSON
     action(
       type="mmjsontransform"
       input="$!qradar"
       output="$!qradar_structured"
       mode="unflatten"
     )

     # Write the structured subtree to a file
     action(
       type="omfile"
       file="/var/log/qradar-structured.json"
       template="outfmt"
     )
   }

How it fits into the log pipeline
---------------------------------
- **Input stage:** receives messages (could be via ``imfile`` or network listener).  
- **Ruleset stage:** parses and transforms JSON as shown.  
- **Action stage:** writes the processed subtree using ``omfile``.  

This mirrors the core architecture introduced in :doc:`index`, showing a
practical transformation flow.

Testing the configuration
-------------------------
1. Inject a sample JSON line containing dotted keys into rsyslog:
   ``logger '{"src.ip":"10.0.0.5","dst.ip":"10.0.0.6"}'``
2. Verify output:
   ``tail -n1 /var/log/qradar-structured.json``  
   should show nested JSON:
   ``"src": {"ip": "10.0.0.5"}, "dst": {"ip": "10.0.0.6"}``
3. Validate syntax: ``rsyslogd -N1``

Practical notes
---------------
- Use ``useRawMsg="on"`` when uncertain if a syslog header is present.
- Guard transformations with ``$parsesuccess`` to avoid malformed results.
- Subtree templates keep the output clean and deterministic.
- The ``unflatten`` mode handles dotted keys automatically, preserving hierarchy.

Conclusion
----------
This example demonstrates how rsyslog’s modular log pipeline can perform
structured transformations inline.  
The same pattern can extend to larger architectures — for example, parsing
JSON at the edge and forwarding structured logs via RELP or Kafka.

See also
--------
- :doc:`stages`
- :doc:`design_patterns`
- :doc:`troubleshooting`
- :doc:`../../configuration/modules/mmjsonparse`
- :doc:`../../configuration/modules/mmjsontransform`
- :doc:`../../configuration/modules/omfile`
