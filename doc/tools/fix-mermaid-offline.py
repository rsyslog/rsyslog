#!/usr/bin/env python3
"""
Fix Mermaid offline viewing by converting ES modules to non-module builds.
This script modifies generated HTML files to work with file:// URLs and
HTTP-served docs (e.g. GitHub Pages PR previews).
"""

import os
import re
import sys
from pathlib import Path

# Path to Mermaid UMD in Sphinx _static (relative to HTML root)
MERMAID_STATIC_PATH = "_static/vendor/mermaid/mermaid.min.js"


def _static_relpath(html_path: Path, html_file: Path) -> str:
    """Return relative path from html_file to _static (e.g. '../' for development/foo.html)."""
    try:
        rel = html_file.relative_to(html_path)
    except ValueError:
        return ""
    depth = len(rel.parts) - 1  # -1 because the file itself is the last part
    if depth <= 0:
        return ""
    return "../" * depth


def fix_mermaid_offline(html_dir):
    """Fix Mermaid ES modules for offline viewing."""
    html_path = Path(html_dir)
    
    if not html_path.exists():
        print(f"Error: Directory {html_dir} does not exist")
        return False
    
    # Find all HTML files
    html_files = list(html_path.rglob("*.html"))
    
    if not html_files:
        print(f"No HTML files found in {html_dir}")
        return False
    
    print(f"Found {len(html_files)} HTML files to process...")
    
    fixed_count = 0
    
    for html_file in html_files:
        try:
            with open(html_file, 'r', encoding='utf-8') as f:
                content = f.read()
            
            original_content = content
            relpath = _static_relpath(html_path, html_file)
            mermaid_src = relpath + MERMAID_STATIC_PATH
            
            # 1. Remove ELK layout script first (before step 2). If we did step 2 first,
            #    its regex would also match mermaid-layout-elk.min.js, strip type="module",
            #    and then this step would no longer see the tag to remove.
            content = re.sub(
                r'\s*<script type="module" src="[^"]*mermaid-layout-elk[^"]*"></script>\n?',
                '',
                content
            )
            
            # 2. Remove type="module" from Mermaid UMD script (monkey-patched build)
            content = re.sub(
                r'<script type="module" src="([^"]*mermaid[^"]*\.min\.js[^"]*)"',
                r'<script src="\1"',
                content
            )
            
            # 3. Replace inline ESM import with UMD script (unpatched sphinxcontrib-mermaid)
            #    e.g. <script type="module">import mermaid from "vendor/mermaid/mermaid.min.js";
            #    The import path "vendor/..." is wrong for HTTP; use correct _static path.
            esm_import_pat = re.compile(
                r'<script type="module">\s*import mermaid from ["\']vendor/mermaid/mermaid\.min\.js["\'];\s*\n',
                re.IGNORECASE
            )
            if esm_import_pat.search(content):
                content = esm_import_pat.sub(
                    f'<script src="{mermaid_src}"></script>\n    <script>',
                    content,
                    count=1
                )
            
            # 4. Add fallback script for offline viewing - only once per file
            content = re.sub(
                r'(<script src="[^"]*mermaid[^"]*\.min\.js[^"]*"></script>)(?!\s*\n\s*<script>console\.info)',
                r'\1\n    <script>console.info("Mermaid offline mode: Basic diagrams supported");</script>',
                content,
                count=1
            )
            
            # Only write if content changed
            if content != original_content:
                with open(html_file, 'w', encoding='utf-8') as f:
                    f.write(content)
                fixed_count += 1
                print(f"Fixed: {html_file.relative_to(html_path)}")
        
        except Exception as e:
            print(f"Error processing {html_file}: {e}")
    
    print(f"\nFixed {fixed_count} HTML files for offline Mermaid viewing")
    print("Note: ELK layout functionality is not available in offline mode")
    return True


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 doc/tools/fix-mermaid-offline.py <html_directory>")
        print("Example: python3 doc/tools/fix-mermaid-offline.py doc/build")
        sys.exit(1)
    
    html_dir = sys.argv[1]
    
    if fix_mermaid_offline(html_dir):
        print("\n✅ Mermaid offline fix completed successfully!")
        print("You can now open HTML files directly in your browser without CORS issues.")
    else:
        print("\n❌ Mermaid offline fix failed!")
        sys.exit(1)


if __name__ == "__main__":
    main()
