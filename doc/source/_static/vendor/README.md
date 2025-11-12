# Vendored JavaScript assets

The files in this directory provide offline copies of the JavaScript
dependencies required by the Sphinx documentation when rendering
Mermaid diagrams.

| Package | Version | Upstream source | License |
|---------|---------|-----------------|---------|
| d3 | 7.9.0 | https://cdn.jsdelivr.net/npm/d3@7.9.0/dist/d3.min.js | [BSD-3-Clause](https://github.com/d3/d3/blob/main/LICENSE) |
| mermaid | 11.2.0 | https://cdn.jsdelivr.net/npm/mermaid@11.2.0/dist/mermaid.min.js | [MIT](https://github.com/mermaid-js/mermaid/blob/develop/LICENSE) |
| @mermaid-js/layout-elk | 0.1.4 | https://cdn.jsdelivr.net/npm/@mermaid-js/layout-elk@0.1.4/dist/mermaid-layout-elk.esm.min.mjs | [MIT](https://github.com/mermaid-js/mermaid-layout-elk/blob/main/LICENSE) |

These files are checked into the repository to keep the generated HTML
documentation self-contained and compliant with packaging policies that
forbid loading assets from external CDNs.

## Offline Viewing

The documentation uses non-module JavaScript builds to ensure compatibility
with `file://` URLs. You can open the generated HTML files directly in your
browser without requiring a local HTTP server.

**Note**: ELK layout functionality is not available in offline mode due to
browser CORS restrictions on ES modules. Basic Mermaid diagrams will still
render correctly.

### Fixing Offline Compatibility

The documentation is automatically fixed for offline viewing during the build
process. If you need to run the fix manually:

```bash
python3 tools/fix-mermaid-offline.py doc/build/html
```

This script removes the `type="module"` attribute from Mermaid script tags
to ensure compatibility with `file://` URLs.
