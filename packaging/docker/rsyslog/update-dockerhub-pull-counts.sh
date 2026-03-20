#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
summary_csv="$script_dir/dockerhub_pull_counts_rsyslog.csv"
history_csv="$script_dir/dockerhub_pull_counts_rsyslog_history.csv"
chart_svg="$script_dir/dockerhub_pull_counts_rsyslog_history.svg"

snapshot_at_utc=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
snapshot_day=$(date -u +"%Y-%m-%d")

summary_tmp=$(mktemp)
history_tmp=$(mktemp)
trap 'rm -f "$summary_tmp" "$history_tmp"' EXIT

cat >"$summary_tmp" <<'EOF'
scope,repo,published_on_dockerhub,current_pull_count,first_public_tag_pushed_utc,days_since_first_public_tag_as_of_snapshot_day,avg_pulls_per_day_since_first_public_tag,last_updated_utc,dockerhub_repo_url,dockerhub_repo_api_url,dockerhub_tags_api_url,notes
EOF

if [[ -f "$history_csv" ]]; then
  cp "$history_csv" "$history_tmp"
else
  cat >"$history_tmp" <<'EOF'
snapshot_at_utc,snapshot_day_utc,scope,repo,published_on_dockerhub,pull_count,last_updated_utc,first_public_tag_pushed_utc,dockerhub_repo_url,dockerhub_repo_api_url,dockerhub_tags_api_url,notes
EOF
fi

repos=(
  "current_user_facing|rsyslog|Standard image"
  "current_user_facing|rsyslog-minimal|Minimal base image"
  "current_user_facing|rsyslog-collector|Collector image"
  "current_user_facing|rsyslog-dockerlogs|Docker log collection image"
  "current_user_facing|rsyslog-etl|Local subtree exists but Docker Hub repository currently returns 404"
  "legacy_alt_repo|official-docker|Shown on org page but not one of the current subtree image names"
  "legacy_alt_repo|official-collector|Shown on org page but not one of the current subtree image names"
  "legacy_alt_repo|official-minimal|Shown on org page but not one of the current subtree image names"
)

csv_escape() {
  local value=${1:-}
  value=${value//\"/\"\"}
  printf '"%s"' "$value"
}

append_summary_row() {
  local scope=$1 repo=$2 published=$3 pull_count=$4 first_pushed=$5 days=$6 avg=$7
  local last_updated=$8 repo_url=$9 repo_api=${10} tags_api=${11} notes=${12}
  {
    csv_escape "$scope"; printf ','
    csv_escape "rsyslog/$repo"; printf ','
    csv_escape "$published"; printf ','
    csv_escape "$pull_count"; printf ','
    csv_escape "$first_pushed"; printf ','
    csv_escape "$days"; printf ','
    csv_escape "$avg"; printf ','
    csv_escape "$last_updated"; printf ','
    csv_escape "$repo_url"; printf ','
    csv_escape "$repo_api"; printf ','
    csv_escape "$tags_api"; printf ','
    csv_escape "$notes"; printf '\n'
  } >>"$summary_tmp"
}

append_history_row() {
  local scope=$1 repo=$2 published=$3 pull_count=$4 last_updated=$5
  local first_pushed=$6 repo_url=$7 repo_api=$8 tags_api=$9 notes=${10}
  {
    csv_escape "$snapshot_at_utc"; printf ','
    csv_escape "$snapshot_day"; printf ','
    csv_escape "$scope"; printf ','
    csv_escape "rsyslog/$repo"; printf ','
    csv_escape "$published"; printf ','
    csv_escape "$pull_count"; printf ','
    csv_escape "$last_updated"; printf ','
    csv_escape "$first_pushed"; printf ','
    csv_escape "$repo_url"; printf ','
    csv_escape "$repo_api"; printf ','
    csv_escape "$tags_api"; printf ','
    csv_escape "$notes"; printf '\n'
  } >>"$history_tmp"
}

days_since() {
  local iso_ts=$1
  local day_part=${iso_ts%%T*}
  local now_epoch start_epoch
  now_epoch=$(date -u -d "$snapshot_day" +%s)
  start_epoch=$(date -u -d "$day_part" +%s)
  echo $(( (now_epoch - start_epoch) / 86400 + 1 ))
}

avg_per_day() {
  local pulls=$1 days=$2
  awk -v pulls="$pulls" -v days="$days" 'BEGIN { printf "%.1f", pulls / days }'
}

for entry in "${repos[@]}"; do
  IFS='|' read -r scope repo notes <<<"$entry"
  repo_url="https://hub.docker.com/r/rsyslog/$repo"
  repo_api="https://hub.docker.com/v2/namespaces/rsyslog/repositories/$repo/"
  tags_api="https://hub.docker.com/v2/namespaces/rsyslog/repositories/$repo/tags?page_size=100"

  published=no
  pull_count=
  first_public_tag_pushed_utc=
  days_since_first_public_tag=
  avg_pulls_per_day=
  last_updated_utc=

  if repo_json=$(curl -fsSL "$repo_api" 2>/dev/null); then
    published=yes
    pull_count=$(jq -r '.pull_count // empty' <<<"$repo_json")
    last_updated_utc=$(jq -r '.last_updated // empty' <<<"$repo_json")
    if tags_json=$(curl -fsSL "$tags_api" 2>/dev/null); then
      first_public_tag_pushed_utc=$(jq -r '
        [.results[]?.tag_last_pushed // empty]
        | sort
        | .[0] // empty
      ' <<<"$tags_json")
      if [[ -n "$first_public_tag_pushed_utc" ]]; then
        days_since_first_public_tag=$(days_since "$first_public_tag_pushed_utc")
        avg_pulls_per_day=$(avg_per_day "$pull_count" "$days_since_first_public_tag")
      fi
    fi
  fi

  append_summary_row \
    "$scope" "$repo" "$published" "$pull_count" "$first_public_tag_pushed_utc" \
    "$days_since_first_public_tag" "$avg_pulls_per_day" "$last_updated_utc" \
    "$repo_url" "$repo_api" "$tags_api" "$notes"

  append_history_row \
    "$scope" "$repo" "$published" "$pull_count" "$last_updated_utc" \
    "$first_public_tag_pushed_utc" "$repo_url" "$repo_api" "$tags_api" "$notes"
done

mv "$summary_tmp" "$summary_csv"
mv "$history_tmp" "$history_csv"
node "$script_dir/generate-dockerhub-pull-chart.mjs" \
  "$history_csv" "$chart_svg"

printf 'Updated %s, %s and %s at %s\n' \
  "$summary_csv" "$history_csv" "$chart_svg" "$snapshot_at_utc"
