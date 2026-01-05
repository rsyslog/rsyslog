# AGENTS.md – omelasticsearch output module

## Module overview
- Ships events to Elasticsearch via HTTP bulk requests using libcurl.
- User documentation: `doc/source/configuration/modules/omelasticsearch.rst`.
- Support status: core-supported. Maturity: fully-mature.

## Build & dependencies
- **Efficient Build:** Use `make -j$(nproc) check TESTS=""` to build the module and test dependencies.
- **Configure:** Run `./configure --enable-elasticsearch` (and other needed flags).
- **Bootstrap:** Only run `./autogen.sh` if you touch `configure.ac`, `Makefile.am`, or `m4/`.

## Local testing
- **Do not run the integration tests during routine agent work.** They require
  downloading and launching an Elasticsearch node, which is too heavy for the
  sandbox environment.
- Compiling the module is sufficient validation. Run the efficient build command above.
- Maintainers who need full coverage can enable the suite with
  `--enable-elasticsearch-tests=minimal` and run `make es-basic.log` (additional
  scenarios live beside it), but expect multi-minute startup overhead for the
  embedded Elasticsearch instance.

## Diagnostics & troubleshooting
- impstats exposes the `omelasticsearch` counter set (submitted, fail.http,
  fail.httprequests, fail.es, and the retry-focused `response.*` metrics when
  `retryfailures="on"`).
- Configure `errorfile="/path/to/errors.json"` while testing to capture raw
  Elasticsearch responses for triage.
- Watch for `writeoperation` validation failures; tests like
  `es-writeoperation.sh` enforce accepted values.

## Cross-component coordination
- Template changes that alter JSON structure must be mirrored in
  `doc/source/configuration/modules/omelasticsearch.rst` and in the
  parameter reference snippets under `doc/source/reference/parameters/`.
- Updates that touch the curl helper patterns should be reviewed alongside
  other HTTP-based plugins (e.g., `contrib/omhttp`).

## Metadata & housekeeping
- Keep `plugins/omelasticsearch/MODULE_METADATA.yaml` current when ownership or
  maturity changes.
- Update `doc/ai/module_map.yaml` and relevant tests if new configuration
  options or diagnostics are introduced.
