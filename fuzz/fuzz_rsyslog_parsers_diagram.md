
```mermaid

flowchart TD
    A["File/AFL++ or libFuzzer input"] --> B["shared driver initialization"]
    B --> C["rs_fuzz_init()"]
    B --> D["rs_fuzz_dispatch_input()"]
    D --> E["analyze_input(data)"]

    subgraph Data_Driven_Routing
        E -->|RFC3164/5424-ish| F["fuzz_parse_target + fuzz_ruleset_target"]
        E -->|JSON markers| G["fuzz_json_props_target<br/>fuzz_json_accessor_target<br/>fuzz_json_msgset_target<br/>fuzz_template_json_target"]
        E -->|structured data| H["fuzz_structured_data_target"]
        E -->|template cues| I["fuzz_template_target<br/>fuzz_template_dynamic_target"]
        E -->|timestamp-ish| J["fuzz_timestamp_target<br/>fuzz_timestamp_format_target"]
        E -->|config keywords| K["fuzz_cfsysline_target"]
        E -->|RFC or newline| U["fuzz_tcp_framing_target"]
        E -->|newline| V["fuzz_imfile_line_target"]
        E -->|queue/config token| W["fuzz_queue_lifecycle_target"]
        E -->|template cues| X["fuzz_action_format_target"]
    end

    D --> L{"Selector fallback<br/>bytes, size, and hash"}
    L -->|ruleset| F1["fuzz_ruleset_target"]
    L -->|template| I1["fuzz_template_target"]
    L -->|template JSON| G1["fuzz_template_json_target"]
    L -->|timestamp| J1["fuzz_timestamp_target"]
    L -->|JSON props/accessor/msgset| G2["fuzz_json_*"]
    L -->|structured data| H1["fuzz_structured_data_target"]
    L -->|cfsysline| K1["fuzz_cfsysline_target"]
    L -->|default| F2["fuzz_parse_target"]

    subgraph MessageLifeCycle
        F & F1 & F2 & G & G1 & G2 & H & I & I1 --> M["msgConstruct()"]
        M --> N["parser.ParseMsg / msgAddJSON / MsgSetPropsViaJSON / tplToString / tplToJSON"]
        N --> O["msgDestruct()"]
    end

    J & J1 --> P["datetime.ParseTIMESTAMP*<br/>formatTimestamp*"]
    K & K1 --> Q["processCfSysLineCommand()"]
    U --> U1["tcps_sess.DataRcvd()<br/>in-memory submit callback"]
    V --> V1["temporary file line split<br/>parse + template formatting"]
    W --> W1["direct/linked-list/disk queues<br/>construct + enqueue + destruct"]
    X --> X1["named template formatting<br/>temporary output file"]

    O --> R{"Crash?"}
    P --> R
    Q --> R
    U1 --> R
    V1 --> R
    W1 --> R
    X1 --> R
    R -->|No| S["Loop continues"]
    R -->|Yes| T["Report to AFL++"]

    style A fill:#e1f5fe
    style G fill:#fff3e0
    style G2 fill:#fff3e0
    style T fill:#ffebee

```

## Description

- File mode, AFL++ persistent mode, and libFuzzer use the same dispatcher and
  iteration reset path.
- Initialization happens once; each testcase can reach multiple subsystems
  based on its contents, including parser/ruleset, JSON, structured data,
  templates, timestamps, TCP framing, queues, imfile-like line processing,
  action formatting, and legacy configuration commands.
- Four byte/length selectors plus a deterministic hash keep target selection
  diverse for inputs without recognizable syntax.
- `msgConstruct`/`msgDestruct` wrap each target to avoid leaks/double-frees; datetime formatting and cfsysline parsing are driven in isolation.
- Runs under AFL++ persistent mode for fast iterations.
