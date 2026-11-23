import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import fs from 'node:fs';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const args = new Map();
for (let i = 2; i < process.argv.length; i += 2) {
  assert.ok(['--host', '--worker', '--output'].includes(process.argv[i]) && process.argv[i + 1]);
  args.set(process.argv[i], process.argv[i + 1]);
}
assert.ok(args.has('--host') && args.has('--worker'));
const retained = args.has('--output');
const output = retained
  ? path.resolve(args.get('--output'))
  : fs.mkdtempSync(path.join(os.tmpdir(), 'capsid-cli-guard-'));
if (retained) fs.mkdirSync(output);
const bundle = path.join(output, 'bundle.mjs');
fs.writeFileSync(bundle, "export default {fetch(){return new Response('cli-guard-healthy')}};\n");
const appConfig = path.join(output, 'capsid.json');
fs.writeFileSync(
  appConfig,
  JSON.stringify({
    apiVersion: 'capsid/app-v2',
    entry: 'bundle.mjs',
    request: {
      timeout: '10s',
      maxInflight: 4,
    },
    pool: {
      workers: 1,
      queueRequests: 4,
      queueTimeout: '1s',
    },
  }),
);
const hostConfig = path.join(output, 'host.json');
fs.writeFileSync(
  hostConfig,
  JSON.stringify({
    apiVersion: 'capsid/host-v2',
    paths: {
      applications: path.join(output, 'apps'),
      state: path.join(output, 'state'),
      secrets: path.join(output, 'secrets', '{application}'),
    },
    admin: { unix: path.join(output, 'admin.sock') },
    capacity: {
      workersTotal: 2,
      channelsTotal: 1024,
      maxChannelMessageSize: '1MiB',
      channelIdleTimeout: '60s',
    },
    maximums: {
      pool: { workers: 2, queueRequests: 16, queueTimeout: '10s' },
      request: {
        timeout: '60s',
        maxInflight: 8,
      },
    },
    listeners: [
      {
        name: 'public',
        tcp: '127.0.0.1:0',
        publicOrigin: 'http://public.example',
        routing: { mode: 'path' },
        limits: {
          channels: 128,
        },
        cors: {
          allowedOrigins: ['https://client.example'],
          allowedMethods: ['GET', 'POST'],
          allowedHeaders: ['X-Test'],
          maxAge: '60s',
        },
      },
    ],
  }),
);
const record = { complete: false, cases: [], mode: 'actual-host-executable' };
const save = () =>
  fs.writeFileSync(path.join(output, 'run.json'), JSON.stringify(record, null, 2) + '\n');
save();

async function health(port) {
  return new Promise((resolve, reject) => {
    const request = http.get(
      { host: '127.0.0.1', port, path: '/@capsid/orders/', agent: false },
      (response) => {
        const body = [];
        response.on('data', (chunk) => body.push(chunk));
        response.on('error', reject);
        response.on('end', () => {
          try {
            assert.equal(response.statusCode, 200);
            assert.equal(Buffer.concat(body).toString(), 'cli-guard-healthy');
            resolve();
          } catch (error) {
            reject(error);
          }
        });
      },
    );
    request.on('error', reject);
    request.setTimeout(3000, () => request.destroy(new Error('health request timeout')));
  });
}

async function launch(name, values) {
  const child = spawn(args.get('--host'), values, {
    stdio: ['ignore', 'pipe', 'pipe', 'pipe', 'pipe'],
  });
  const stdout = [],
    stderr = [],
    readyBytes = [];
  child.stdout.on('data', (chunk) => stdout.push(chunk));
  child.stderr.on('data', (chunk) => stderr.push(chunk));
  let forcedStop = false;
  const timer = setTimeout(() => {
    forcedStop = true;
    child.kill('SIGTERM');
  }, 8000);
  const hardTimer = setTimeout(() => child.kill('SIGKILL'), 10000);
  const exited = new Promise((resolve, reject) => {
    child.once('error', reject);
    child.once('close', (code, signal) => resolve({ code, signal }));
  });
  const ready = new Promise((resolve) => {
    let text = '';
    child.stdio[3].on('data', (chunk) => {
      readyBytes.push(chunk);
      text += chunk.toString();
      const end = text.indexOf('\n');
      if (end >= 0) resolve(JSON.parse(text.slice(0, end)));
    });
  });
  let startup;
  let exit;
  try {
    startup = await Promise.race([ready, exited.then(() => null)]);
    if (startup) {
      assert.equal(startup.schema, 'capsid-host-ready-v1');
      await health(startup.port);
    }
  } finally {
    child.stdio[4].destroy();
    try {
      exit = await exited;
    } finally {
      clearTimeout(timer);
      clearTimeout(hardTimer);
    }
    fs.writeFileSync(path.join(output, name + '.stdout.log'), Buffer.concat(stdout));
    fs.writeFileSync(path.join(output, name + '.stderr.log'), Buffer.concat(stderr));
  }
  assert.equal(forcedStop, false, 'a timeout cannot count as rejection');
  const result = {
    name,
    command: [args.get('--host'), ...values],
    ready: startup,
    exit,
    readyBytes: Buffer.concat(readyBytes).length,
    stdoutBytes: Buffer.concat(stdout).length,
    stderr: Buffer.concat(stderr).toString(),
    forcedStop,
  };
  record.cases.push(result);
  save();
  return result;
}

try {
  for (const workers of [1, 2]) {
    const label = `workers-${workers}`;
    const base = [
      '--app-dir',
      output,
      '--app',
      'orders',
      '--host-config',
      hostConfig,
      '--listen',
      '127.0.0.1:0',
      '--worker',
      args.get('--worker'),
      '--workers',
      String(workers),
      '--strict-sandbox',
      'off',
      '--ready-fd',
      '3',
      '--stop-fd',
      '4',
    ];
    const positive = await launch(label + '-known-options', base);
    assert.ok(
      positive.ready,
      'valid documented arguments must still start and serve a real request',
    );
    assert.deepEqual(positive.exit, { code: 0, signal: null });
    for (const name of [
      'parent-stop-fd',
      'request-timout-ms',
      'max-inflight-per-workre',
      'totally-unknown',
      'source-bundle',
      'routing',
      'capsid-json',
      'cors-origins',
    ]) {
      for (const invalidPaths of [false, true]) {
        const values = [...base];
        if (invalidPaths) {
          values[values.indexOf('--worker') + 1] = path.join(output, 'absent-worker');
          values[values.indexOf('--app-dir') + 1] = path.join(output, 'absent-app-dir');
        }
        values.push('--' + name, '1');
        const result = await launch(
          label + '-' + name + '-' + (invalidPaths ? 'before-io' : 'full-config'),
          values,
        );
        assert.equal(
          result.ready,
          null,
          'unknown argument was accepted and a real worker served HTTP',
        );
        assert.deepEqual(result.exit, { code: 2, signal: null });
        assert.equal(result.readyBytes, 0);
        assert.equal(result.stdoutBytes, 0);
        const lines = result.stderr.trim().split('\n');
        assert.equal(lines.length, 1, 'argument rejection emits one structured startup diagnostic');
        const diagnostic = JSON.parse(lines[0]);
        assert.equal(diagnostic.result, 'fail');
        assert.equal(
          diagnostic.message,
          'unknown argument: --' + name,
          'rejection must precede worker/bundle I/O diagnostics',
        );
      }
    }
  }
  for (const legacyMode of ['single-worker', 'static-pool']) {
    const result = await launch(`legacy-mode-${legacyMode}`, [
      '--mode',
      legacyMode,
      '--app-dir',
      path.join(output, 'absent-app-dir'),
      '--worker',
      path.join(output, 'absent-worker'),
    ]);
    assert.equal(result.ready, null);
    assert.deepEqual(result.exit, { code: 2, signal: null });
    assert.equal(result.readyBytes, 0);
    assert.equal(result.stdoutBytes, 0);
    const diagnostic = JSON.parse(result.stderr.trim().split('\n')[0]);
    assert.equal(diagnostic.result, 'fail');
    assert.match(diagnostic.message, /^unknown --mode /);
  }
  record.status = 'complete';
  console.log(`host CLI unknown-argument validation: PASS cases=${record.cases.length}`);
} catch (error) {
  record.status = 'failed';
  record.failure = String(error.stack ?? error);
  throw error;
} finally {
  record.complete = true;
  save();
  if (!retained) {
    const directory = fs.realpathSync(output);
    assert.equal(path.dirname(directory), fs.realpathSync(os.tmpdir()));
    assert.ok(path.basename(directory).startsWith('capsid-cli-guard-'));
    fs.rmSync(directory, { recursive: true, force: true });
  }
}
