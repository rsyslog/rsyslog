#!/usr/bin/env node

import fs from 'node:fs';

const [, , historyPath, outputPath] = process.argv;

if (!historyPath || !outputPath) {
  console.error('usage: generate-dockerhub-pull-chart.mjs <history.csv> <out.svg>');
  process.exit(1);
}

const csv = fs.readFileSync(historyPath, 'utf8').trim();
if (!csv) {
  console.error(`empty CSV: ${historyPath}`);
  process.exit(1);
}

function parseCsvLine(line) {
  const out = [];
  let cur = '';
  let inQuotes = false;
  for (let i = 0; i < line.length; i += 1) {
    const ch = line[i];
    if (inQuotes) {
      if (ch === '"' && line[i + 1] === '"') {
        cur += '"';
        i += 1;
      } else if (ch === '"') {
        inQuotes = false;
      } else {
        cur += ch;
      }
    } else if (ch === '"') {
      inQuotes = true;
    } else if (ch === ',') {
      out.push(cur);
      cur = '';
    } else {
      cur += ch;
    }
  }
  out.push(cur);
  return out;
}

const lines = csv.split('\n').filter(Boolean);
const headers = parseCsvLine(lines[0]);
const rows = lines.slice(1).map((line) => {
  const cols = parseCsvLine(line);
  return Object.fromEntries(headers.map((key, idx) => [key, cols[idx] ?? '']));
});

const chartRepos = [
  { repo: 'rsyslog/rsyslog', color: '#0b6e4f', label: 'rsyslog' },
  { repo: 'rsyslog/rsyslog-collector', color: '#c44536', label: 'collector' },
  { repo: 'rsyslog/rsyslog-minimal', color: '#1f4e79', label: 'minimal' },
  { repo: 'rsyslog/rsyslog-dockerlogs', color: '#8a6d1d', label: 'dockerlogs' },
];

const snapshotsByRepo = new Map();
for (const { repo } of chartRepos) {
  snapshotsByRepo.set(repo, new Map());
}

for (const row of rows) {
  if (!snapshotsByRepo.has(row.repo)) {
    continue;
  }
  const pullCount = Number(row.pull_count);
  if (!Number.isFinite(pullCount)) {
    continue;
  }
  snapshotsByRepo.get(row.repo).set(row.snapshot_day_utc, {
    day: row.snapshot_day_utc,
    pullCount,
  });
}

for (const [repo, values] of snapshotsByRepo.entries()) {
  snapshotsByRepo.set(
    repo,
    [...values.values()].sort((a, b) => a.day.localeCompare(b.day)),
  );
}

const allPoints = [...snapshotsByRepo.values()].flat();
if (allPoints.length === 0) {
  console.error('no history rows available for chart');
  process.exit(1);
}

const uniqueDays = [...new Set(allPoints.map((point) => point.day))].sort();
const minDay = uniqueDays[0];
const maxDay = uniqueDays[uniqueDays.length - 1];
const maxPullCount = Math.max(...allPoints.map((point) => point.pullCount), 1);

const width = 1200;
const height = 700;
const margin = { top: 90, right: 220, bottom: 70, left: 90 };
const plotWidth = width - margin.left - margin.right;
const plotHeight = height - margin.top - margin.bottom;

function xForDay(day) {
  if (uniqueDays.length === 1) {
    return margin.left + plotWidth / 2;
  }
  const index = uniqueDays.indexOf(day);
  return margin.left + (index / (uniqueDays.length - 1)) * plotWidth;
}

function yForPullCount(value) {
  return margin.top + plotHeight - (value / maxPullCount) * plotHeight;
}

function esc(text) {
  return text
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;');
}

const gridLines = [];
for (let i = 0; i <= 5; i += 1) {
  const value = Math.round((maxPullCount * i) / 5);
  const y = yForPullCount(value);
  gridLines.push(
    `<line x1="${margin.left}" y1="${y}" x2="${width - margin.right}" y2="${y}" stroke="#d7dfd8" stroke-width="1" />`,
  );
  gridLines.push(
    `<text x="${margin.left - 14}" y="${y + 5}" text-anchor="end" font-size="14" fill="#4f5b52">${value.toLocaleString()}</text>`,
  );
}

const dateTicks = [];
for (let i = 0; i < uniqueDays.length; i += 1) {
  const showTick =
    uniqueDays.length <= 8 ||
    i === 0 ||
    i === uniqueDays.length - 1 ||
    i % Math.ceil(uniqueDays.length / 6) === 0;
  if (!showTick) {
    continue;
  }
  const x = xForDay(uniqueDays[i]);
  dateTicks.push(
    `<line x1="${x}" y1="${margin.top + plotHeight}" x2="${x}" y2="${margin.top + plotHeight + 8}" stroke="#4f5b52" stroke-width="1" />`,
  );
  dateTicks.push(
    `<text x="${x}" y="${height - 24}" text-anchor="middle" font-size="14" fill="#4f5b52">${esc(uniqueDays[i])}</text>`,
  );
}

const linePaths = [];
const legend = [];
chartRepos.forEach((entry, idx) => {
  const points = snapshotsByRepo.get(entry.repo);
  if (!points || points.length === 0) {
    return;
  }
  const pathData = points
    .map((point, pointIdx) => {
      const x = xForDay(point.day);
      const y = yForPullCount(point.pullCount);
      return `${pointIdx === 0 ? 'M' : 'L'} ${x} ${y}`;
    })
    .join(' ');

  linePaths.push(
    `<path d="${pathData}" fill="none" stroke="${entry.color}" stroke-width="3.5" stroke-linecap="round" stroke-linejoin="round" />`,
  );

  for (const point of points) {
    const x = xForDay(point.day);
    const y = yForPullCount(point.pullCount);
    linePaths.push(
      `<circle cx="${x}" cy="${y}" r="4.5" fill="${entry.color}"><title>${esc(`${entry.repo} ${point.day}: ${point.pullCount.toLocaleString()} pulls`)}</title></circle>`,
    );
  }

  const legendY = margin.top + idx * 28;
  const latest = points[points.length - 1].pullCount.toLocaleString();
  legend.push(
    `<line x1="${width - margin.right + 18}" y1="${legendY}" x2="${width - margin.right + 52}" y2="${legendY}" stroke="${entry.color}" stroke-width="4" stroke-linecap="round" />`,
  );
  legend.push(
    `<text x="${width - margin.right + 62}" y="${legendY + 5}" font-size="15" fill="#1f2923">${esc(`${entry.label} (${latest})`)}</text>`,
  );
});

const svg = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" role="img" aria-labelledby="title desc">
  <title id="title">Docker Hub pull count progress for rsyslog container images</title>
  <desc id="desc">Trend chart generated from timestamped Docker Hub pull count snapshots.</desc>
  <rect width="${width}" height="${height}" fill="#f7f5ef" />
  <rect x="${margin.left}" y="${margin.top}" width="${plotWidth}" height="${plotHeight}" fill="#fffdf8" stroke="#d7dfd8" />
  <text x="${margin.left}" y="42" font-size="28" font-family="sans-serif" font-weight="700" fill="#1f2923">Docker Hub Pull Count Progress</text>
  <text x="${margin.left}" y="68" font-size="16" font-family="sans-serif" fill="#4f5b52">Current rsyslog user-facing container images tracked from ${esc(minDay)} to ${esc(maxDay)}</text>
  ${gridLines.join('\n  ')}
  <line x1="${margin.left}" y1="${margin.top + plotHeight}" x2="${width - margin.right}" y2="${margin.top + plotHeight}" stroke="#4f5b52" stroke-width="1.5" />
  <line x1="${margin.left}" y1="${margin.top}" x2="${margin.left}" y2="${margin.top + plotHeight}" stroke="#4f5b52" stroke-width="1.5" />
  ${dateTicks.join('\n  ')}
  ${linePaths.join('\n  ')}
  <text x="26" y="${margin.top + plotHeight / 2}" font-size="16" font-family="sans-serif" fill="#4f5b52" transform="rotate(-90 26 ${margin.top + plotHeight / 2})">Pull count</text>
  <text x="${margin.left + plotWidth / 2}" y="${height - 6}" text-anchor="middle" font-size="16" font-family="sans-serif" fill="#4f5b52">Snapshot day (UTC)</text>
  ${legend.join('\n  ')}
</svg>
`;

fs.writeFileSync(outputPath, svg);
