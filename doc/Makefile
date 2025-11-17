# Makefile for rsyslog-doc
#
# You can set these variables from the command line.
SPHINXOPTS := $(SPHINXOPTS)
SPHINXBUILD ?= sphinx-build
SOURCEDIR = source
BUILDDIR = build

export RSYSLOG_DOC_BUILD_TARGET ?= $(firstword $(MAKECMDGOALS))

# Function to add parallel jobs from MAKEFLAGS to SPHINXOPTS
define add_parallel_jobs
	$(eval SPHINXOPTS += $(if $(filter -j%,$(SPHINXOPTS)),,$(filter -j%,$(MAKEFLAGS))))
endef

.PHONY: help clean html webhtml html-with-sitemap singlehtml json alljson

help:
	@$(call add_parallel_jobs)
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

clean:
	rm -rf "$(BUILDDIR)"

html:
	@$(call add_parallel_jobs)
	@$(SPHINXBUILD) -M html "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
	@echo "Applying Mermaid offline fix..."
	@python3 ./tools/fix-mermaid-offline.py "$(BUILDDIR)/html"

webhtml:
	@$(call add_parallel_jobs)
	@$(SPHINXBUILD) -M html "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
	@echo "Applying Mermaid offline fix..."
	@python3 ./tools/fix-mermaid-offline.py "$(BUILDDIR)/html"

# Build HTML with sitemap enabled (opt-in)
html-with-sitemap:
	@$(call add_parallel_jobs)
	@$(SPHINXBUILD) -M html "$(SOURCEDIR)" "$(BUILDDIR)" -t with_sitemap $(SPHINXOPTS) $(O)
	@echo "Applying Mermaid offline fix..."
	@python3 ./tools/fix-mermaid-offline.py "$(BUILDDIR)/html"

singlehtml:  # -t minimal_build triggers stripped-down config in conf.py
	@$(call add_parallel_jobs)
	@$(SPHINXBUILD) -M singlehtml "$(SOURCEDIR)" "$(BUILDDIR)" -t minimal_build $(SPHINXOPTS) -D rst_epilog='' $(O)
	@echo
	@echo "Build finished. The minimal single page HTML is in $(BUILDDIR)/singlehtml."

json:
	@$(call add_parallel_jobs)
	@$(SPHINXBUILD) -b json "$(SOURCEDIR)" "$(BUILDDIR)/json" $(SPHINXOPTS) $(O)

alljson: json
	@$(call add_parallel_jobs)
	@$(SPHINXBUILD) -b json "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

%:
	@$(call add_parallel_jobs)
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
