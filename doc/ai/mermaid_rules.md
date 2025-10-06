# Mermaid Diagram Rules (MUST FOLLOW)

1. Blank line after `.. mermaid::`
2. Quote all node labels: `A["Input"]`
3. Do not quote edge labels.
4. Use `<br>` for multi-line labels.
5. Prefer 6–10 nodes max.
6. Verify with `make html`.

Example:

```
.. mermaid::

   flowchart LR
     I["Input<br>(imtcp)"]:::input --> R["Ruleset"]:::ruleset --> A["Action<br>(omfile)"]:::action

     classDef input fill:#d5e8d4,stroke:#82b366;
     classDef ruleset fill:#dae8fc,stroke:#6c8ebf;
     classDef action fill:#ffe6cc,stroke:#d79b00;
```
