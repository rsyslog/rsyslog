# Distro Component Package Map

`distro-component-package-map.json` is generated best-effort support data that
maps upstream rsyslog module files to public distribution packages.

The map helps answer a practical security advisory question: if an advisory
names an affected upstream component such as `mmpstrucdata`, which distro
package, if any, ships that component?

## Scope

The generated data is advisory support data only. It does not change the GHSA
package field, which normally identifies the upstream affected product
(`rsyslog`). Advisory titles, summaries, and affected-configuration sections
should still name the precise upstream component and required configuration.

## Caveats

- The map is generated from public package metadata and is not authoritative
  for any distribution.
- `shipped` means public metadata lists the module file in a package.
- `not_shipped` means the configured public metadata sources were searched and
  did not list the module file.
- A shipped module file does not mean the module is loaded by default.
- Custom builds, locally copied modules, third-party repositories, private
  repositories, and downstream patches are outside this map.
- Confirm downstream-specific security claims with the relevant distribution
  security team before publication.

## Refresh

The weekly `distro component package map` workflow regenerates the JSON data
from public distribution metadata and opens a pull request when the checked-in
map changes.
