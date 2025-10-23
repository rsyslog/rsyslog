#!/usr/bin/env python3
"""
Fix Mermaid offline viewing by converting ES modules to non-module builds.
This script modifies generated HTML files to work with file:// URLs.
"""

import os
import re
import sys
from pathlib import Path


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
    
    # Patterns to fix
    fixes = [
        # Remove type="module" from Mermaid scripts (but keep for ELK)
        (
            r'<script type="module" src="([^"]*mermaid[^"]*\.min\.js[^"]*)"',
            r'<script src="\1"'
        ),
        # Add fallback script for offline viewing - only once per file
        (
            r'(<script src="[^"]*mermaid[^"]*\.min\.js[^"]*"></script>)',
            r'\1\n    <script>console.info("Mermaid offline mode: Basic diagrams supported");</script>'
        )
    ]
    
    fixed_count = 0
    
    for html_file in html_files:
        try:
            with open(html_file, 'r', encoding='utf-8') as f:
                content = f.read()
            
            original_content = content
            
            # Apply fixes
            for pattern, replacement in fixes:
                content = re.sub(pattern, replacement, content)
            
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
        print("Example: python3 doc/tools/fix-mermaid-offline.py doc/build/html")
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
