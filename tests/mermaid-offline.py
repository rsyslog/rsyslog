#!/usr/bin/env python3
"""Regression for #7633: legacy and Sphinx-relative Mermaid imports must
become classic script tags with paths relative to each HTML page.
"""

import contextlib
import io
from pathlib import Path
import runpy
import tempfile
import unittest


FIX = runpy.run_path(str(Path(__file__).resolve().parents[1] / "doc/tools/fix-mermaid-offline.py"))


class MermaidOfflineTest(unittest.TestCase):
    def test_import_paths(self):
        cases = (
            ("vendor/mermaid/mermaid.min.js", "index.html", ""),
            ("_static/vendor/mermaid/mermaid.min.js", "index.html", ""),
            ("../_static/vendor/mermaid/mermaid.min.js", "configuration/page.html", "../"),
        )
        for import_path, page_path, prefix in cases:
            with self.subTest(import_path=import_path), tempfile.TemporaryDirectory() as directory:
                page = Path(directory) / page_path
                page.parent.mkdir(parents=True, exist_ok=True)
                initialization = "mermaid.initialize({});\nmermaid.run();"
                page.write_text(
                    f'<script type="module">import mermaid from "{import_path}";\n'
                    f'{initialization}</script>', encoding="utf-8")
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertTrue(FIX["fix_mermaid_offline"](directory))
                result = page.read_text(encoding="utf-8")
                self.assertIn(f'<script src="{prefix}_static/vendor/mermaid/mermaid.min.js"></script>', result)
                self.assertNotIn("import mermaid", result)
                self.assertNotIn('type="module"', result)
                self.assertIn(initialization, result)


if __name__ == "__main__":
    unittest.main()
