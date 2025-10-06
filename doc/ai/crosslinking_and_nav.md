# Cross-linking & Navigation

Use `:doc:` for internal links, not raw URLs.

Add local toctrees for multi-file clusters.

Each concept cluster (e.g., log_pipeline/) must have an index with:
```
.. toctree::
   :maxdepth: 1
   :titlesonly:

   stages
   design_patterns
   example_json_transform
   troubleshooting
```
