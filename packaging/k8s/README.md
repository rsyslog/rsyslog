# Rsyslog on Kubernetes

This directory contains the artifacts needed to build a Kubernetes-native rsyslog image and deploy it in a Kubernetes cluster. This setup is designed to follow modern cloud-native best practices for a production environment.

## Features

*   **Stateful Persistence:** Uses a `StatefulSet` with a `volumeClaimTemplate` to provide persistent storage for the rsyslog message queue, ensuring that no messages are lost if a pod is rescheduled.
*   **Non-Root Container on a Non-Privileged Port:** The container runs as a non-root user (`rsyslog`) and listens on a non-privileged port (`10514`) for enhanced security.
*   **Logging to `stdout`:** Logs are sent to the container's standard output, allowing the Kubernetes logging stack (e.g., Fluentd, Logstash) to collect them.
*   **Health and Liveness Probes:** An HTTP server is exposed on port `8080` with a `/health` endpoint for Kubernetes liveness and readiness probes.
*   **Prometheus Metrics:** The HTTP server also exposes a `/metrics` endpoint with Prometheus-compatible metrics.
*   **Graceful Shutdown:** The container has an entrypoint script that traps `SIGTERM` signals to ensure a graceful shutdown, preventing data loss.
*   **Configurable via `ConfigMap`:** The rsyslog configuration is managed through a `ConfigMap`, allowing for easy customization without rebuilding the container image.
*   **High Availability:** The `StatefulSet` is configured with 2 replicas, and a `PodDisruptionBudget` is provided to ensure at least one replica is available during voluntary disruptions.
*   **Network Security:** A `NetworkPolicy` is included to restrict ingress traffic to the syslog and metrics ports.

## Building the Docker Image

To build the Kubernetes-native rsyslog image, use the provided `Dockerfile.k8s`:

```bash
docker build -t rsyslog/rsyslog-k8s:v8.2025.04.0 -f Dockerfile.k8s .
```

## Deployment

This directory provides a set of example Kubernetes manifests to deploy rsyslog:

*   `rsyslog-configmap.yaml`: A `ConfigMap` containing the `rsyslog.conf` file.
*   `rsyslog-statefulset.yaml`: A `StatefulSet` that manages the rsyslog pods and provides persistent storage.
*   `rsyslog-service.yaml`: A `Service` to expose the syslog (TCP/UDP) and metrics (TCP) ports.
*   `rsyslog-servicemonitor.yaml`: A `ServiceMonitor` for Prometheus integration (requires the Prometheus Operator).
*   `rsyslog-pdb.yaml`: A `PodDisruptionBudget` to ensure high availability.
*   `rsyslog-networkpolicy.yaml`: A `NetworkPolicy` to restrict ingress traffic.

To deploy rsyslog, apply these manifests to your cluster:

```bash
kubectl apply -f rsyslog-configmap.yaml
kubectl apply -f rsyslog-statefulset.yaml
kubectl apply -f rsyslog-service.yaml
kubectl apply -f rsyslog-servicemonitor.yaml # Optional, for Prometheus
kubectl apply -f rsyslog-pdb.yaml
kubectl apply -f rsyslog-networkpolicy.yaml
```

## Configuration

The rsyslog configuration can be modified by editing the `rsyslog-configmap.yaml` file. After applying the changes, you will need to restart the rsyslog pods for the new configuration to take effect:

```bash
kubectl rollout restart statefulset/rsyslog
```

## Observability

### Health Checks

*   **Endpoint:** `/health`
*   **Port:** `8080`

The health endpoint will return a `200 OK` with the body `healthy` if rsyslog is running.

### Metrics

*   **Endpoint:** `/metrics`
*   **Port:** `8080`

The metrics endpoint provides Prometheus-compatible metrics. If you are using the Prometheus Operator, the included `ServiceMonitor` will automatically configure Prometheus to scrape this endpoint.
