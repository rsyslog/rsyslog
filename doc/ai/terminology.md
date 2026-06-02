# Terminology

- **Log pipeline** — canonical (internally *message pipeline*).
- **Input** — e.g., imtcp, imudp, imjournal, imfile.
- **Ruleset** — processing logic.
- **Action** — output destination.
- **Queue** — buffering between stages.

Avoid "data pipeline" unless context demands.

## YAML Configuration

- **YAML front-end** — the translation layer in `runtime/yamlconf.c` that converts
  `.yaml` config files into rsyslog's internal structures.  It has no independent
  runtime path; all execution goes through the shared RainerScript back-end.
- **nvlst** — ordered linked list of `(name, value)` pairs; rsyslog's canonical
  intermediate representation for configuration parameters.  Both the RainerScript
  grammar and the YAML parser produce `nvlst` chains; `nvlstGetParams()` consumes
  them in a format-agnostic way.
- **cnfobj** — a typed wrapper around an `nvlst` chain.  The type tag
  (`CNFOBJ_GLOBAL`, `CNFOBJ_ACTION`, `CNFOBJ_INPUT`, …) tells `cnfDoObj()` which
  module handler to invoke.
- **cnfDoObj()** — the central dispatcher in `rsconf.c` that receives a `cnfobj`
  and calls the appropriate subsystem initialisation function.  Both RainerScript
  and the YAML front-end target this function.
- **cnfAddConfigBuffer()** — pushes a RainerScript text snippet onto the lex buffer
  stack so the running `yyparse()` processes it.  The YAML front-end uses this to
  handle ruleset `script:` / `statements:` bodies without a separate interpreter.

For the full pipeline diagram see `doc/source/development/yaml_config_architecture.rst`.
