// Trusted Binding Runtime #2 ("telemetry")
// Runs in a completely separate QuickJS heap from both the User Runtime and
// the "vault" Binding Runtime, demonstrating M4A fair round-robin scheduling
// and microtask-storm preemption immunity across multiple Bindings.

export default function createTelemetryBinding({ config }) {
  const service = config.service || 'binding-v2-showcase';
  let seq = 0;

  return {
    ping(n, call) {
      return {
        binding: 'telemetry',
        service,
        n,
        seq: ++seq,
        requestId: call.requestId,
      };
    },
  };
}
