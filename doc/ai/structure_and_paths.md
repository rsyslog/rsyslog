# Structure & Paths (Source of Truth)

_Last reviewed: 2025-10-06_

This file mirrors the **current repository layout** (from the uploaded `rsyslog-main.zip`) so AI tools and contributors work from the same reality.

## Top-level doc tree (selected)

```
doc/source/
├── concepts/
├── configuration/
├── getting_started/
├── tutorials/
├── troubleshooting/
├── development/
├── faq/
├── includes/
├── installation/
├── containers/
├── rainerscript/
├── whitepapers/
└── historical/
```

> The landing page is `doc/source/index.rst`. Section hubs include `concepts/index.rst`, `getting_started/index.rst`, and `tutorials/index.rst`.

## Getting Started

**Path:** `doc/source/getting_started/`

Files directly under this directory (selected):
- ai-assistants.rst
- basic_configuration.rst
- forwarding_logs.rst
- index.rst
- installation.rst
- next_steps.rst
- understanding_default_config.rst

### Beginner tutorials

**Path:** `doc/source/getting_started/beginner_tutorials/`

Current files:
- 01-installation.rst
- 02-first-config.rst
- 03-default-config.rst
- 04-message-pipeline.rst
- 05-order-matters.rst
- 06-remote-server.rst
- index.rst

**Notes**
- Tutorials are **numbered** (`NN-title.rst`) and appear in the `beginner_tutorials/index.rst` toctree.
- Use this directory for step-by-step, runnable guides.

## Concepts

**Path:** `doc/source/concepts/`

Files (singletons) in this folder:
- index.rst
- janitor.rst
- messageparser.rst
- multi_ruleset.rst
- netstrm_drvr.rst
- ns_gtls.rst
- ns_ossl.rst
- ns_ptcp.rst
- queues.rst
- rfc5424layers.png

Subdirectories:


**Notes**
- Concept pages explain **architecture and behavior**. Keep them concise and link to tutorials for hands-on examples.
- Multi-file concept clusters (like `log_pipeline/`) should include a **local `.. toctree::`** in their `index.rst` that lists child pages.

## Configuration

**Path:** `doc/source/configuration/`

Top-level files (selected):
- actions.rst
- basic_structure.rst
- conf_formats.rst
- config_param_types.rst
- converting_to_new_format.rst
- cryprov_gcry.rst
- cryprov_ossl.rst
- droppriv.rst
- dyn_stats.rst
- examples.rst
- filters.rst
- index.rst
- ... (+15 more)

### Modules

**Path:** `doc/source/configuration/modules/` — module reference pages (`im*`, `om*`, `mm*`, etc.).

Examples (first 15 of 99 files):
- gssapi.png
- gssapi.rst
- idx_input.rst
- idx_library.rst
- idx_messagemod.rst
- idx_output.rst
- idx_parser.rst
- idx_stringgen.rst
- im3195.rst
- imbatchreport.rst
- imczmq.rst
- imdocker.rst
- imdtls.rst
- imfile.rst
- imgssapi.rst
- ... (+84 more)

### Ruleset / Actions / Global / Input directives
Other subfolders under `configuration/` group specific topics:
- action
- global
- input_directives
- modules
- ruleset

## Tutorials

**Path:** `doc/source/tutorials/`

Files:
- cert-script.tar.gz
- database.rst
- failover_syslog_server.rst
- gelf_forwarding.rst
- hash_sampling.rst
- high_database_rate.rst
- index.rst
- log_rotation_fix_size.rst
- log_sampling.rst
- random_sampling.rst
- recording_pri.rst
- reliable_forwarding.rst
- tls.rst
- tls_cert.jpg
- tls_cert_100.jpg
- tls_cert_ca.jpg
- tls_cert_ca.rst
- tls_cert_client.rst
- tls_cert_errmsgs.rst
- tls_cert_machine.rst
- tls_cert_scenario.rst
- tls_cert_script.rst
- tls_cert_server.rst
- tls_cert_summary.rst
- tls_cert_udp_relay.rst

**Notes**
- Use this for longer-form or specialized how-tos that don’t fit the beginner sequence.

## ToC integration rules (must follow)

- Every new page must be included in **some `.. toctree::`** or Sphinx will warn that it’s not in any toctree.
- Section hubs that own ToCs:
  - `concepts/index.rst`
  - `getting_started/beginner_tutorials/index.rst`
  - `tutorials/index.rst`
- For **multi-file** concept clusters add a local toctree in the cluster’s `index.rst`:
  ```
  .. toctree::
     :maxdepth: 1
     :titlesonly:

     stages
     design_patterns
     example_json_transform
     troubleshooting
  ```

## Placement cheatsheet

- **Beginner, runnable guides** → `getting_started/beginner_tutorials/`
- **Architecture & theory** → `concepts/` (create a subdir if multi-page)
- **Module reference** → `configuration/modules/`
- **Advanced how-tos** → `tutorials/`
- **RainerScript language** → `rainerscript/`

## Mermaid & authoring references

See `/doc/ai/mermaid_rules.md` for diagram rules and `/doc/ai/authoring_guidelines.md` for RST style (anchors, meta, summary slice).


