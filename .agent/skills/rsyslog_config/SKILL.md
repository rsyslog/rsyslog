---
name: rsyslog_config
description: Governs the dual-frontend config architecture (RainerScript + YAML) and enforces parity rules when extending or changing configuration objects.
---

# rsyslog_config

rsyslog supports two configuration frontends that share a single backend:

- **RainerScript** — parsed by `grammar/` (flex/bison), produces `cnfobj` + `nvlst` trees.
- **YAML** — parsed by `runtime/yamlconf.c` (libyaml), produces the same `cnfobj` + `nvlst` trees.

Both call `cnfDoObj()` → module `setModCnf()` / `newActInst()` with identical data structures.
This means **any change to the config layer must be reflected in both frontends**.

## Quick Start

1. New config object or statement → update `grammar/` *and* `runtime/yamlconf.c`.
2. New module parameter → add YAML smoke test alongside the RainerScript test.
3. New template modifier or property → update `parse_template_sequence()` in `yamlconf.c`.
4. Changed semantics → update `doc/source/configuration/yaml_config.rst` and the module `.rst`.

## Detailed Instructions

### 1. Architecture: one backend, two frontends

```
rsyslog.conf  ──► grammar/ (flex/bison) ──┐
                                           ├──► cnfobj + nvlst ──► cnfDoObj() ──► module setModCnf()
config.yaml   ──► runtime/yamlconf.c   ──┘
```

The backend entry point is `cnfDoObj()` in `runtime/conf.c`.  Both frontends
must produce structurally identical `nvlst` parameter lists for a given object
so the module receives the same data regardless of which format the operator
chose.

### 2. Config parity rules

| Change | RainerScript | YAML | Test |
|--------|-------------|------|------|
| New global parameter | `grammar/rainerscript.y` | `parse_global()` in yamlconf.c | `yaml-global-*.sh` |
| New module parameter | module param table | no change needed — nvlst key matches param name | `yaml-<module>-*.sh` |
| New statement type | grammar rule + action | `build_one_stmt_rs()` in yamlconf.c | `yaml-statements-*.sh` |
| New block type (ruleset, etc.) | grammar rule | `parse_<type>_sequence()` in yamlconf.c | `yaml-<type>-*.sh` |
| New template modifier | `template.c` modifier table | `parse_template_sequence()` in yamlconf.c | `yaml-template-*.sh` |
| New template property option | `template.c` | `parse_template_sequence()` in yamlconf.c | existing template tests |

### 3. Key files

| File | Role |
|------|------|
| `runtime/yamlconf.c` | YAML frontend — all YAML parsing lives here |
| `runtime/yamlconf.h` | Public API: `yamlconf_load()` |
| `grammar/rainerscript.y` | RainerScript grammar |
| `grammar/rainerscript.h` | RainerScript AST types |
| `runtime/conf.c` | Shared backend: `cnfDoObj()`, `cnfDoCfsysline()` |
| `runtime/nvlst.c` | Parameter list nodes shared by both frontends |
| `doc/source/configuration/yaml_config.rst` | User-facing YAML reference |

### 4. Adding a new statement type

**RainerScript** — add a grammar rule in `grammar/rainerscript.y` that calls the
appropriate runtime function.

**YAML** — add a branch in `build_one_stmt_rs()` in `runtime/yamlconf.c`:
```c
} else if (!strcmp(kname, "mystatement")) {
    /* parse value(s), build nvlst node(s), call runtime function */
}
```

**Test** — add or extend a `tests/yaml-statements-*.sh` that exercises the new
statement and verifies observable output.

### 5. Adding a new template modifier or property option

Modifiers (e.g. `uppercase`, `json`, `csv`) and per-property options (e.g.
`name`, `format`, `onEmpty`, `position.from`, `position.to`) are decoded in
`parse_template_sequence()` inside `yamlconf.c`.  When `template.c` gains a new
modifier or option, add the matching YAML key there and add a test case to
`tests/yaml-template-*.sh`.

### 6. YAML test conventions

- Name: `tests/yaml-<area>-<what>.sh` (e.g. `yaml-template-list.sh`,
  `yaml-global-ratelimit.sh`).
- Source `diag.sh` and use the standard helpers (`startup`, `injectmsg`,
  `wait_queueempty`, `shutdown_when_empty`, `wait_shutdown`).
- Config file: `tests/testsuites/<testname>.yaml` (`.yaml` extension triggers
  the YAML loader).
- Verify observable output — don't just check that rsyslog starts; assert that
  the expected data appears in output files.
- Register in `tests/Makefile.am` under the same conditionals as the module
  being tested.

### 7. Documentation

Every config object documented in `doc/source/configuration/modules/` or
`doc/source/reference/parameters/` should include a YAML example alongside the
RainerScript example.  Update `doc/source/configuration/yaml_config.rst` when:
- A new statement type or global option is added.
- Supported template modifiers or property options change.

## Related Skills

- `rsyslog_module`: Concurrency and module lifecycle patterns.
- `rsyslog_test`: Testbench conventions and Makefile.am registration.
- `rsyslog_doc`: Documentation structure and Sphinx validation.
