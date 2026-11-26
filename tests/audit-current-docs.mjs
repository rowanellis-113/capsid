import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { access, readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import { archivedQualification } from './documentation_archive.mjs';

const root = path.resolve(process.argv[2] ?? '.');

async function markdownFiles(relativeRoot) {
  const absoluteRoot = path.join(root, relativeRoot);
  const entries = await readdir(absoluteRoot, { withFileTypes: true });
  const result = [];
  for (const entry of entries) {
    if (entry.name === 'node_modules') {
      continue;
    }
    const relativePath = path.join(relativeRoot, entry.name);
    if (entry.isDirectory()) {
      result.push(...(await markdownFiles(relativePath)));
    } else if (entry.isFile() && entry.name.endsWith('.md')) {
      result.push(relativePath);
    }
  }
  return result;
}

const documentPaths = [
  'README.md',
  'ROADMAP.md',
  'CONTRIBUTING.md',
  'SECURITY.md',
  'bench/README.md',
  'bench/SPEC.md',
  'bench/perf-evidence.md',
  'perf-site/README.md',
  ...(await markdownFiles('docs')),
  ...(await markdownFiles('examples')),
]
  .map((value) => value.split(path.sep).join('/'))
  .sort();

const coverage = JSON.parse(
  await readFile(path.join(root, 'docs/data/documentation-coverage.json'), 'utf8'),
);
assert.equal(coverage.schema, 1);

// The readable English editions occupy their original paths. A published release
// retains the exact captures, including the line endings used by the evidence logs.
const capturedBytes = new Map();
const captureRevision = '845ae5dee5833c1bed1c2630cfb65e6ed76af7d7'; // v0.3.1-beta.1
for (const entry of coverage.documents.filter((doc) => doc.kind === 'historical-translation')) {
  assert.match(entry.path, /^docs\/(?:evidence\/v0\.3\.0|history\/archives)\//);
  const original = execFileSync(
    'git',
    ['-C', root, 'cat-file', 'blob', `${captureRevision}:${entry.path}`],
    { maxBuffer: 4 * 1024 * 1024 },
  );
  assert.equal(createHash('sha256').update(original).digest('hex'), entry.sourceSha256, entry.path);
  capturedBytes.set(entry.path, original);
}

const documents = new Map();
let archivedSnapshots = 0;
for (const relativePath of documentPaths) {
  const bytes = await readFile(path.join(root, relativePath));
  if (await archivedQualification(root, relativePath, capturedBytes.get(relativePath) ?? bytes)) {
    archivedSnapshots += 1;
    continue;
  }
  documents.set(
    relativePath,
    // Normalize CRLF checkouts (git autocrlf on Windows) so the frozen
    // fragment matching below is line-ending agnostic.
    bytes.toString('utf8').replace(/\r\n/g, '\n'),
  );
}

// Mixed current-reference/journal pages were migrated without rewriting the
// captured source. Keep the original frozen contract checks against exact bytes,
// while current pages describe the delivered release rather than old M1 status.
const archivedPages = coverage.documents.filter((entry) => entry.archivedSource);
const pageForArchive = new Map(archivedPages.map((entry) => [entry.archivedSource, entry.path]));
assert.equal(pageForArchive.size, archivedPages.length, 'each archived source has one entry page');
const sourceSnapshots = new Map();
const migration = JSON.parse(
  await readFile(path.join(root, 'docs/history/archives/2026-11-17/index.json'), 'utf8'),
);
assert.equal(migration.schema, 1, 'known historical-document migration schema');
for (const snapshot of migration.snapshots) {
  assert.equal(
    snapshot.originalPath,
    `docs/${path.posix.basename(snapshot.path, '.txt')}`,
    'an archived journal retains its original source identity',
  );
  const page = pageForArchive.get(snapshot.path);
  const target = path.resolve(root, snapshot.path);
  assert.ok(
    target.startsWith(path.join(root, 'docs/history/archives/2026-11-17') + path.sep),
    'historical source stays in its inventoried archive',
  );
  assert.ok(
    documents.has(page) && !sourceSnapshots.has(page),
    'one historical source per maintained entry page',
  );
  const bytes = capturedBytes.get(snapshot.path) ?? (await readFile(target));
  assert.equal(bytes.length, snapshot.bytes, 'historical source length is exact');
  assert.equal(
    createHash('sha256').update(bytes).digest('hex'),
    snapshot.sha256,
    'historical source bytes were not rewritten',
  );
  sourceSnapshots.set(page, bytes.toString('utf8').replace(/\r\n/g, '\n'));
}
assert.equal(sourceSnapshots.size, 6, 'all six mixed journals retain captured source');
assert.equal(sourceSnapshots.size, pageForArchive.size, 'all inventoried archives were verified');

function localLinks(relativePath, content) {
  const links = [];
  for (const match of content.matchAll(/\[[^\]]+\]\(([^)]+)\)/g)) {
    let target = match[1].trim();
    if (
      target.startsWith('http://') ||
      target.startsWith('https://') ||
      target.startsWith('mailto:') ||
      target.startsWith('#')
    ) {
      continue;
    }
    if (target.startsWith('<') && target.endsWith('>')) {
      target = target.slice(1, -1);
    }
    target = decodeURIComponent(target.split('#', 1)[0]);
    if (!target) {
      continue;
    }
    links.push({
      original: match[1],
      resolved: path.resolve(root, path.dirname(relativePath), target),
    });
  }
  return links;
}

for (const [relativePath, content] of documents) {
  for (const link of localLinks(relativePath, content)) {
    await assert.doesNotReject(
      access(link.resolved),
      `${relativePath} has a broken local link: ${link.original}`,
    );
  }
}

// Every maintained docs/*.md file must be reachable from the docs index. This
// permits a small hierarchy (for example the framework index) without allowing
// an unindexed orphan document to accumulate.
const docsPaths = [...documents.keys()].filter((value) => value.startsWith('docs/'));
const docsEntries = await readdir(path.join(root, 'docs'), { withFileTypes: true });
assert.deepEqual(
  docsEntries
    .filter((entry) => !entry.name.startsWith('.'))
    .map((entry) => entry.name)
    .sort(),
  ['README.md', 'data', 'design', 'evidence', 'guides', 'history', 'reference'],
  'file documentation under its subject directory and keep only the index at the top level',
);
for (const name of docsPaths.filter((p) => /^docs\/(guides|reference|design)\//.test(p))) {
  assert.match(
    name,
    /^docs\/(?:[a-z0-9]+(?:-[a-z0-9]+)*\/)+(?:README|[a-z0-9]+(?:-[a-z0-9]+)*)\.md$/,
    'use lowercase hyphenated subject names, or README.md for an index',
  );
}
const reachable = new Set(['docs/README.md']);
const pending = ['docs/README.md'];
while (pending.length > 0) {
  const source = pending.pop();
  for (const link of localLinks(source, documents.get(source))) {
    const relativeTarget = path.relative(root, link.resolved).split(path.sep).join('/');
    if (
      relativeTarget.startsWith('docs/') &&
      relativeTarget.endsWith('.md') &&
      documents.has(relativeTarget) &&
      !reachable.has(relativeTarget)
    ) {
      reachable.add(relativeTarget);
      pending.push(relativeTarget);
    }
  }
}
assert.deepEqual(
  docsPaths.filter((value) => !reachable.has(value)),
  [],
  'docs/README.md does not reach every maintained Markdown document',
);

const capabilityManifest = JSON.parse(
  await readFile(path.join(root, 'docs/data/capability-manifest.json'), 'utf8'),
);
const readme = documents.get('README.md');
const contributing = documents.get('CONTRIBUTING.md');
const securityPolicy = documents.get('SECURITY.md');
const architecture = documents.get('docs/design/runtime-architecture.md');
const hostDesign = documents.get('docs/design/host-architecture.md');
const performanceGuide = documents.get('docs/reference/benchmarks.md');
const testingGuide = documents.get('docs/guides/run-tests.md');
const capabilityPolicy = documents.get('docs/reference/capability-policy.md');
const modulePermissions = documents.get('docs/reference/module-permissions.md');
for (const moduleName of capabilityManifest.modules.built_and_available) {
  assert.ok(
    readme.includes(`\`${moduleName}\``),
    `README.md does not list current module ${moduleName}`,
  );
  assert.ok(
    capabilityPolicy.includes(`\`${moduleName}\``),
    `capability-policy.md does not explain current module ${moduleName}`,
  );
  assert.ok(
    modulePermissions.includes(`\`${moduleName}\``),
    `module-permissions.md does not explain current module ${moduleName}`,
  );
}
for (const permissionName of Object.keys(capabilityManifest.permissions)) {
  assert.ok(
    modulePermissions.includes(`\`${permissionName}\``),
    `module-permissions.md does not explain permission ${permissionName}`,
  );
}

for (const requiredFragment of [
  'cmake --install build-release',
  '<capsid/runtime.hpp>',
  'When `egress_policy == NULL`, all egress Fetch requests are denied.',
  '`strict_sandbox` is off by default',
  '`tjs:*` modules cannot be enabled through configuration',
  '`CAPSID_EVENT_READY.flags` contains the sandbox features required by the deployment',
]) {
  assert.ok(
    readme.includes(requiredFragment),
    `README.md is missing user-facing configuration guidance: ${requiredFragment}`,
  );
}

assert.ok(
  !readme.includes('v0.2.0-rc.04'),
  'README.md still points users at a release that did not carry install.sh',
);
for (const [documentName, content] of [
  ['CONTRIBUTING.md', contributing],
  ['SECURITY.md', securityPolicy],
]) {
  assert.ok(
    content.includes('`0.2.2`') && content.includes('`0.3.1-beta`'),
    `${documentName} must distinguish stable from the experimental prerelease`,
  );
}
assert.ok(
  contributing.includes('examples/elysia-reference'),
  'CONTRIBUTING.md omits the Elysia example dependency install',
);
assert.ok(
  architecture.includes('Elysia 1.4.29'),
  'docs/design/runtime-architecture.md omits the currently validated Elysia version',
);
for (const staleClaim of [
  'The current tree has no benchmark runner',
  'The current tree has no `bench/`',
]) {
  assert.ok(
    !hostDesign.includes(staleClaim),
    `docs/design/host-architecture.md contains stale current-state claim: ${staleClaim}`,
  );
}

for (const [documentName, content, requiredFragments] of [
  [
    'docs/design/runtime-architecture.md',
    architecture,
    [
      'Platform support splits into two independent commitments: native development and production isolation.',
      'The Windows native development toolchain has been delivered since v0.1.2.',
      'The host decides which capabilities it needs and verifies the actual features at READY.',
      'The runtime implements process creation, IPC, termination/reclamation, and OS sandboxing.',
    ],
  ],
  [
    'docs/design/host-architecture.md',
    hostDesign,
    [
      'not part of the M1 release gate; M1A',
      'M1A + M1B are delivered as one implementation batch',
      'M1A: the benchmark-minimal single-worker data plane',
      'Run the first 15.7 baseline immediately after green',
      'TSan may come after that first baseline',
      'must precede M1C acceptance and the M2 multi-worker implementation',
      'M1C/M1D',
      'call `capsid_worker_fd()` directly',
      'not implemented on Windows machines/hosted runners',
      'seccomp/Landlock bits express different guarantees',
    ],
  ],
  [
    'docs/guides/run-tests.md',
    documents.get('docs/guides/run-tests.md'),
    [
      'TSan (M1C gate)',
      'must pass before M1C acceptance',
      'Do not mix with ASan, UBSan, LTO, fuzz, or benchmark runs',
      'Any report from first-party code fails',
      'personality(ADDR_NO_RANDOMIZE)',
      'seccomp=unconfined',
    ],
  ],
  [
    'docs/reference/benchmarks.md',
    performanceGuide,
    [
      'the first round does not wait for request body, streaming, cancel, or timeout to be implemented',
      'Once those contracts land, they must be benchmarked on the same runner',
    ],
  ],
  [
    'docs/guides/run-tests.md',
    testingGuide,
    [
      'Windows native-dev uses the `windows-latest` hosted runner',
      'Windows cross-compilation, Wine, WSL2, or a Linux container cannot substitute for hosted native Windows execution evidence',
      'All five job classes produce hosted evidence',
    ],
  ],
]) {
  const contractContent = sourceSnapshots.get(documentName) ?? content;
  for (const fragment of requiredFragments) {
    assert.ok(
      contractContent.includes(fragment),
      `${documentName} is missing the frozen platform contract: ${fragment}`,
    );
  }
}

const removedDocuments = [
  'docs/host-architecture-plan.md',
  'docs/project-status.md',
  'docs/txiki-upgrade-report.md',
  'docs/txiki-upgrade-report.json',
];
for (const relativePath of removedDocuments) {
  await assert.rejects(
    access(path.join(root, relativePath)),
    `${relativePath} is a removed snapshot or superseded document`,
  );
}

const currentDocs = [...documents.values()].join('\n');
for (const staleFragment of [
  'host-architecture-plan.md',
  'project-status.md',
  'txiki-upgrade-report.md',
  'bench/README.md',
  '](../bench/results/',
  'vue-ssr-sweep-r1-20260730',
  'app-v2.ts',
  'app-v3.ts',
  'add_argument("--rounds"',
  '--rounds 3',
  'other ordinary extensions remain unavailable',
]) {
  assert.ok(
    !currentDocs.includes(staleFragment),
    `current documentation contains stale fragment: ${staleFragment}`,
  );
}

const currentStatus = JSON.parse(
  await readFile(path.join(root, 'docs/data/current-status.json'), 'utf8'),
);
const profileV2 = JSON.parse(
  await readFile(path.join(root, 'tests/wpt/workerd-default-profile-v2.json'), 'utf8'),
);
assert.equal(currentStatus.developmentBranch, 'dev/v0.3.1');
assert.equal(currentStatus.prerelease.channel, '0.3.1-beta');
assert.equal(currentStatus.prerelease.tag, 'v0.3.1-beta.1');
assert.equal(currentStatus.prerelease.draft, false);
assert.equal(currentStatus.prerelease.prerelease, false);
assert.equal(currentStatus.prerelease.githubLatest, true);
assert.equal(currentStatus.githubLatest.tag, 'v0.3.1-beta.1');
for (const [label, release] of [
  ['Stable', currentStatus.stable],
  ['Prerelease', currentStatus.prerelease],
]) {
  assert.ok(
    readme.includes(`> **${label}**: [\`${release.tag}\`](${release.url})`),
    `README.md must link the recorded ${label.toLowerCase()} release explicitly`,
  );
}
const sdkHeader = await readFile(path.join(root, 'include/capsid/runtime.h'), 'utf8');
const abiVersion = sdkHeader.match(/^#define CAPSID_ABI_VERSION (\d+)u$/m)[1];
assert.ok(readme.includes(`v0.3.2 development SDK uses C ABI v${abiVersion}.`));
const embeddingApi = documents.get('docs/reference/embedding-api.md');
assert.ok(
  embeddingApi.includes(`The v0.3.2 SDK uses ABI v${abiVersion},`),
  `docs/reference/embedding-api.md banner must match CAPSID_ABI_VERSION=${abiVersion}`,
);
assert.ok(
  embeddingApi.includes(`The current \`CAPSID_ABI_VERSION == ${abiVersion}\``),
  `docs/reference/embedding-api.md must state CAPSID_ABI_VERSION == ${abiVersion}`,
);
const { repository } = JSON.parse(
  await readFile(path.join(root, 'docs/data/release-status.json'), 'utf8'),
);
const installer = currentStatus.prerelease.assets.find((asset) => asset.name === 'install.sh');
assert.ok(
  readme.includes(`curl -fsSL ${installer.browser_download_url}`) &&
    readme.includes(`| CAPSID_REPO=${repository} bash -s -- ${currentStatus.prerelease.tag}`),
  'install the tagged prerelease from the recorded repository instead of relying on defaults',
);
assert.equal(currentStatus.runtime.sourceCommit, currentStatus.prerelease.sourceCommit);
assert.equal(currentStatus.runtime.sourceTree, currentStatus.prerelease.sourceTree);
assert.equal(currentStatus.runtime.publicAbi, 7);
assert.equal(currentStatus.runtime.defaultWireVersion, 3);
assert.equal(currentStatus.checkout.kind, 'test-harness-successor');
assert.equal(currentStatus.checkout.baseCommit, currentStatus.prerelease.sourceCommit);
assert.deepEqual(Object.keys(currentStatus.checkout.buildInputChanges), [
  'cmake/build_tests.cmake',
]);
assert.equal(Object.keys(currentStatus.checkout.runtimeInputChanges).length, 0);
assert.equal(Object.keys(currentStatus.checkout.vendorPatchChanges).length, 0);
assert.equal(
  currentStatus.checkout.releaseQualificationInherited,
  false,
  'test-harness successors do not inherit binary qualification beyond the tagged release',
);
assert.match(currentStatus.runtime.sourceScope, /qualified 0\.3\.1-beta interface baseline/);
assert.equal(currentStatus.conformance.referenceProfile, profileV2.id);
assert.deepEqual(
  currentStatus.conformance.rawFailingFiles.map((value) => value.path),
  profileV2.conflicts.map((value) => value.path),
);
assert.equal(currentStatus.qualification.ordinarySuitePassed, false);
assert.equal(currentStatus.qualification.standardsConformanceClaimed, false);
assert.equal(currentStatus.performance.measurementBuild, 'local-rebuild');
assert.equal(currentStatus.performance.matchesPublishedBetaSource, false);
assert.equal(currentStatus.performance.sourceCommit, '6d4cc616619dfd1ada0e484c2c47b79046d680dc');
assert.equal(currentStatus.performance.sourceTag, 'v0.3.1-beta.1');
assert.equal(currentStatus.performance.representsPublishedRelease, false);
assert.equal(
  currentStatus.performance.publishedBetaPerformanceMeasured,
  false,
  'the dev/v0.3.1 measurement is not relabeled as a published release asset benchmark',
);
assert.ok(
  documents.get('docs/reference/web-conformance.md').includes('workerd-default-profile-v2.json'),
);
assert.ok(
  documents.get('docs/reference/embedding-api.md').includes('capsid_worker_channel_confirm'),
);

const coveredDocs = coverage.documents.filter(
  (value) => value.path.startsWith('docs/') && value.path.endsWith('.md'),
);
assert.deepEqual(
  coveredDocs.map((value) => value.path).sort(),
  documentPaths.filter((value) => value.startsWith('docs/')).sort(),
  'every docs page has a review disposition',
);
for (const document of coverage.documents) {
  assert.ok(
    [
      'current-reference',
      'historical-plan-or-journal',
      'historical-translation',
      'immutable-evidence',
      'release-history',
    ].includes(document.kind),
  );
  assert.equal(document.reviewed, true, `${document.path}: review is incomplete`);
  assert.ok(
    ['exact-bytes', 'canonical-lf-text'].includes(document.hashMode),
    `${document.path}: known coverage digest semantics`,
  );
  const bytes = await readFile(path.join(root, document.path));
  const input =
    document.hashMode === 'exact-bytes'
      ? bytes
      : Buffer.from(bytes.toString('utf8').replace(/\r\n/g, '\n'), 'utf8');
  assert.equal(
    createHash('sha256').update(input).digest('hex'),
    document.sha256,
    `${document.path}: coverage data is stale`,
  );
}
const actualKindCounts = {};
for (const document of coverage.documents) {
  actualKindCounts[document.kind] = (actualKindCounts[document.kind] ?? 0) + 1;
}
assert.deepEqual(
  coverage.counts,
  actualKindCounts,
  'documentation-coverage.json counts must match per-kind document totals',
);

console.log(
  `current docs: ${documents.size} documents, ${archivedSnapshots} archived snapshots hash-checked, ` +
    `${capabilityManifest.modules.built_and_available.length} modules verified`,
);
