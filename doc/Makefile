# Makefile for rsyslog-doc
#
# You can set these variables from the command line.
SPHINXOPTS ?=
SPHINXBUILD ?= sphinx-build
SOURCEDIR = source
BUILDDIR = build

.PHONY: help clean html singlehtml

help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

clean:
	rm -rf "$(BUILDDIR)"

html:
	@$(SPHINXBUILD) -M html "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

singlehtml:  # -t minimal_build triggers stripped-down config in conf.py
	@$(SPHINXBUILD) -M singlehtml "$(SOURCEDIR)" "$(BUILDDIR)" -t minimal_build $(SPHINXOPTS) -D rst_epilog='' $(O)
	@echo
	@echo "Build finished. The minimal single page HTML is in $(BUILDDIR)/singlehtml."

%:
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
