.. _dev_architecture:

System Architecture
===================

.. page-summary-start

High-level architectural overview of rsyslog, describing the microkernel pattern,
core components, and data flow pipeline.

.. page-summary-end

Rsyslog follows a **Microkernel-like Architecture** (also known as a Plugin Architecture).
The core system provides essential services—threading, queueing, configuration parsing,
and object management—while the actual business logic of receiving, processing,
and sending logs is implemented in dynamically loadable modules.

Architectural Patterns
----------------------

Microkernel (Core vs. Plugins)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The system is cleanly divided into two layers:

1.  **The Core (Runtime):**
    Located in ``runtime/``, this acts as the "kernel." It knows nothing about
    TCP, files, or Elasticsearch. It manages:
    -   **Resources:** Worker threads (``wtp.c``), memory, and queues.
    -   **Orchestration:** Loading plugins, parsing `rsyslog.conf`, and routing messages.
    -   **Interfaces:** Defines the contract (``modules.h``) that all plugins must obey.

2.  **The Plugins (Extensions):**
    Located in ``plugins/`` (and some legacy ones in ``tools/``), these provide
    functionality. They are classified by their prefix:
    -   **Input (`im`):** Producers (e.g., `imtcp`, `imfile`).
    -   **Output (`om`):** Consumers (e.g., `omelasticsearch`, `omfile`).
    -   **Parser (`pm`):** Decoders (e.g., `pmrfc5424`).
    -   **Message Modification (`mm`):** Transformers (e.g., `mmjsonparse`).
    -   **Function (`fm`):** Extension functions for RainerScript.

Data Flow Pipeline
------------------
The system operates as an **Event-Driven, Multi-threaded Producer-Consumer Pipeline**.

.. mermaid::

   flowchart LR
     Source(("Log Source")) --> Input["Input Module<br>(imtcp)"]
     Input -->|smsg_t| PreProc["Preprocessing"]
     PreProc --> Queue["Main Queue<br>(qqueue_t)"]
     Queue -->|Batch| Parser["Parsers"]
     Parser --> Rules["Ruleset Engine"]
     Rules -->|Filter| ActionQ["Action Queue"]
     ActionQ --> Output["Output Module<br>(omfile)"]
     Output --> Dest(("Destination"))

1.  **Input (Producer):** An input module runs a thread (often an infinite loop like `runInput`) to listen for data. It creates a **Message Object** (``smsg_t``) and submits it to the core.
2.  **Queue (Buffer):** The core ``qqueue_t`` buffers messages in memory or on disk to handle bursts and ensure reliability.
3.  **Processing (Orchestration):** Worker threads dequeue messages. The parser chain decodes the raw message. The ruleset engine evaluates filters.
4.  **Action (Consumer):** If a filter matches, the message is passed to an Action. The action invokes an Output Module via a standard interface (``doAction`` or Transactional).

Component Map
-------------

- **``tools/``**: Contains the **Entry Point** (``rsyslogd.c``) and legacy built-in modules (e.g., `omfile.c`, `ompipe.c`).
- **``runtime/``**: The **Core Engine**.
    -   ``msg.c``: The Message object.
    -   ``queue.c``: Queue implementation (In-Memory, Disk, DA).
    -   ``modules.c``: Plugin loader and interface definitions.
    -   ``obj.c``: The internal Object System.
- **``plugins/``**: The **Extension Ecosystem**. Contains the majority of modern modules.
- **``contrib/``**: Community-contributed modules. These are functionally identical to plugins but have different maintenance guarantees.

The Object System
-----------------
Rsyslog implements a custom **Object System** in C (``runtime/obj.h``) to provide encapsulation and polymorphism.

-   **Macros:** It relies heavily on macros like ``DEFobjCurrIf`` (Define Object Current Interface) and ``BEGINobjInstance`` to simulate classes.
-   **Lifecycle:** Objects follow a strict constructor/destructor pattern:
    -   ``Construct(&pThis)``: Allocate memory.
    -   ``SetProperty(pThis, ...)``: Configure the instance.
    -   ``ConstructFinalize(pThis)``: Ready the instance for use.
    -   ``Destruct(&pThis)``: Teardown and free.
