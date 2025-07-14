> [`rsyslog/rsyslog-docker`](https://github.com/rsyslog/rsyslog-docker) repository.**  
> It was merged into this monorepo on 2025-07-12 to consolidate related components
> and simplify development. All Docker-related build and image definitions
> are now maintained here as part of rsyslog’s unified repository structure.

# Rsyslog Docker Images

This repository contains the Dockerfiles and build configurations for creating official Docker images for rsyslog. Our goal is to provide robust, flexible, and easy-to-use containerized rsyslog solutions for various logging needs.

---
## User-Focused Docker Images (Ubuntu Based)

We provide a set of user-focused Docker images based on **Ubuntu LTS**, utilizing the Adiscon PPA for the latest stable rsyslog versions. These images are designed with a layered approach and offer runtime configurability.

**Current Development Status & Feedback:**
*Please note that this new set of user-facing containers is currently under active development and improvement. While they are intended to be stable and safe for use, rapid changes and enhancements are expected during this phase. We highly appreciate user feedback! If you encounter any issues or have suggestions, please open an issue on our GitHub repository: [https://github.com/rsyslog/rsyslog-docker/issues](https://github.com/rsyslog/rsyslog-docker/issues).*

### Naming Convention & Tagging

Our user-focused images follow this naming and tagging scheme:

* **Standard Image:** `rsyslog/rsyslog:<version>`
    * Example: `rsyslog/rsyslog:2025-04`
    * The `:latest` tag for `rsyslog/rsyslog` will point to the most recent build of this standard image.
* **Functional Variant Images:** `rsyslog/rsyslog-<function>:<version>`
    * Example: `rsyslog/rsyslog-minimal:2025-04`
    * Example: `rsyslog/rsyslog-collector:2025-04`
    * Each functional variant repository (e.g., `rsyslog/rsyslog-minimal`) will also have its own `:latest` tag pointing to its most recent build.

The `<version>` tag corresponds to the rsyslog version, which follows a `YYYY-MM` scheme and is released bi-monthly (e.g., "2025-04" denotes the rsyslog version released in April 2025).

### Available Image Variants

1.  **`rsyslog/rsyslog:<version>` (Standard Image)**
    * **Base:** Builds upon the `minimal` image.
    * **Contents:** Includes the rsyslog core plus a curated set of commonly used modules (e.g., `imhttp` for health checks/metrics, `omhttp`). This image is intended as the default, general-purpose choice for most users.
    * **Purpose:** Suitable for a wide range of logging tasks, offering a good balance of features and footprint.

2.  **`rsyslog/rsyslog-minimal:<version>` (Minimal Image)**
    * **Base:** Leanest Ubuntu LTS.
    * **Contents:** Rsyslog core binaries and essential runtime files only. No optional modules are included by default.
    * **Purpose:** For users requiring absolute control over installed components, wishing to build highly specialized custom images, or operating in extremely resource-constrained environments.

3.  **`rsyslog/rsyslog-collector:<version>` (Collector Image)**
    * **Base:** Builds upon the `standard` image.
    * **Contents:** Includes modules typically needed for a log collector/aggregator role, such as `rsyslog-elasticsearch`, `rsyslog-omkafka`, `rsyslog-relp`, and common input modules (TCP, UDP, RELP).
    * **Purpose:** Optimized for centralized log ingestion and forwarding to various storage or analysis backends.

4.  **`rsyslog/rsyslog-dockerlogs:<version>` (Docker Logs Image)**
    * **Base:** Builds upon the `standard` image.
    * **Contents:** Includes `rsyslog-imdocker` and configurations tailored for collecting logs from the Docker daemon and containers.
    * **Purpose:** Specialized for environments where rsyslog is used to gather and process logs originating from Docker.

5.  **`rsyslog/rsyslog-debug:<version>` (Debug Image - Planned)**
    * **Base:** Builds upon the `standard` image.
    * **Contents:** Includes debugging tools and potentially rsyslog compiled with extra debug symbols.
    * **Purpose:** For troubleshooting and development purposes.

*(Note: Specific modules included in `standard`, `collector`, and other variants might evolve. Refer to the individual Dockerfiles for the most up-to-date package lists.)*

### Runtime Configuration

Many features and modules within these images (especially `standard` and above) can be enabled or disabled at runtime using environment variables in conjunction with rsyslog's `config.enable=\`cat $ENVVAR\`` directive in the rsyslog configuration. This allows for flexible deployments without needing a custom image for every minor configuration change.

---
## Building the Images

The `Makefile` in the /rsyslog path of this repository is used to build these Docker images.

* To build all defined user-focused images (minimal, standard, collector, dockerlogs):
    ```bash
    make all
    ```
* To build a specific image variant:
    ```bash
    make standard
    make minimal
    make collector
    make dockerlogs
    ```
* The rsyslog version for the image tags can be overridden:
    ```bash
    make VERSION=your-custom-version-tag all
    ```
    By default, it uses the version specified in the `Makefile`.
* To force a rebuild without using Docker cache:
    ```bash
    make REBUILD=yes all
    ```

---
## Development and CI Images

* `/dev_env` - This directory contains Docker images used as build environments for rsyslog Continuous Integration (CI) testing. They contain every dependency required for building and testing rsyslog and are consequently several gigabytes in size. These images are meant for rsyslog contributors and developers, not for general public use as runtime containers.

---
## Historical Information (For Reference Only)

The following sections describe previous efforts and image bases that are **no longer maintained or produced**. They are kept for historical reference.

### Discontinued Appliance Experiment
* `/appliance` - This directory represents an early experiment at creating an all-in-one rsyslog logging appliance. This approach did not align with current containerization best practices and was discontinued. The images are outdated and should not be used.

### Legacy Base Images
* Legacy Alpine and CentOS base images previously existed in a `/base` directory. These are no longer maintained. Development now focuses exclusively on Ubuntu-based images, which integrate best with our daily stable package builds.

### CentOS Notes (Historical)
* Older CentOS 7 definitions can be found under `/base/centos7`. Like the Alpine files, these are no longer maintained but remain for reference.

### Historical: Alpine Linux Build Process
The following notes describe the original Alpine Linux based build process. These images are **no longer built or supported**.

We initially intended to use Alpine Linux for its small size and security profile. However, Alpine often missed components required by rsyslog, necessitating that we build and maintain custom packages. This became an ongoing effort as rsyslog versions advanced.

* For those interested, the old custom packaging repository is still available at:
    [https://github.com/rgerhards/alpine-rsyslog-extras](https://github.com/rgerhards/alpine-rsyslog-extras).

* **Package Build Environment (Historical Alpine):**
    We used a modified version of `docker-alpine-abuild` for building custom Alpine packages:
    [https://github.com/rgerhards/docker-alpine-abuild/tree/master-rger](https://github.com/rgerhards/docker-alpine-abuild/tree/master-rger)
    This was based on `andyshinn/docker-alpine-abuild` with rsyslog-specific tweaks, including our own unofficial APK repository for newer dependencies.

* **Bootstrap (Historical Alpine):**
    *(Note: "usr" below refers to a user-specific prefix used during the old build process.)*
    The process to bootstrap the Alpine package building from scratch involved:
    1.  Creating a `usr/docker-alpine-abuild` image, initially *without* our custom repository.
    2.  Building an `autotools-archive` package via `usr/alpine-linux-extras`.
    3.  Copying this package to a custom HTTP server (our APK repository).
    4.  Rebuilding `usr/docker-alpine-abuild` image, this time *with* the custom repository enabled to use the newly built dependencies.
    5.  Building the rest of the custom packages in `usr/alpine-linux-extras`, with rsyslog generally being built last. This often required multiple uploads to the custom repository as dependencies were met.

---
## Important Note
**Remember to periodically update your Docker images to include the latest (security) updates from the base OS and rsyslog itself!** Always prefer using specific version tags for production deployments rather than `:latest` to ensure stability.
