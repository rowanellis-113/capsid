# Capsid v0.3.2 工程审校指引（供独立 AI 审校）

> **基线与分支界定**：当前已交付的稳定版为 `v0.2.2`，实验性预发布版为 `v0.3.1-beta.1`（发布基线与已测量证据以 [当前状态](current-status.md) 为准）。本指引面向开发分支 `dev/v0.3.2` 相对 `a7fd47e` 新增的 4 个核心提交（`a7fd47e..e44d6ad`），对照 [Config v2 与弹性池设计方案](config-v2-design.md) 进行独立对抗式代码审校（Adversarial Code Review）。

---

## 0. 审校入口与核心要求

请以**发现真实缺陷与结构性风险**为唯一目标（并发竞态、死锁、容量/账本泄漏、TOCTOU、安全越界、与设计文档偏差、兼容性破坏、测试盲区），避免泛泛总结。每条审校意见都必须给出**精确的源文件路径、行号范围（或 commit SHA）及触发时序推导**。

先在仓库根目录运行以下命令建立全局视图：

```sh
git log --oneline a7fd47e..e44d6ad
git diff --stat a7fd47e..e44d6ad
```

- 仓库范围：`E:/capsid`（或对应的 Linux / WSL2 同步源目录 `/home/eroszhao/capsid-src-v032`），分支 `dev/v0.3.2`
- 提交区间：`a7fd47e..e44d6ad`（共 4 个 commit，122 个文件变更，`+14068 / -14813`）

---

## 1. 四阶段提交与核心代码映射

| 阶段 | Commit SHA | 主题与涉及范围 | 核心文件入口 |
| --- | --- | --- | --- |
| Stage 0 | `c060e78` | 前置去重重构、Worker 密度加固与 `ManagedListener` 数据面能力补齐 | [json_codec.h](../src/host/json_codec.h)、[units.h](../src/host/units.h)、[net_target.h](../src/net_target.h)、[ipc_validation.h](../src/ipc_validation.h)、[worker_cgroup.h](../src/host/worker_cgroup.h)、[managed_listener.cc](../src/host/managed_listener.cc) |
| Stage 1 | `1910e64` | 正交 `capsid/host-v2` / `capsid/app-v2` 配置与跨平台同构 `LocalDevHost` | [config.cc](../src/host/config.cc)、[host_config_model.cc](../src/host/host_config_model.cc)、[policy_compiler.cc](../src/host/policy_compiler.cc)、[local_dev_host.h](../src/host/local_dev_host.h)、[local_dev_host.cc](../src/host/local_dev_host.cc)、[main.cc](../src/host/main.cc)、[ws_host_backend.mjs](../tests/helpers/ws_host_backend.mjs) |
| Stage 2 | `d02a60e` | 应用弹性池扩缩容（含 `minReady = 0` Scale-to-Zero）、双层账本与异步回收 | [worker_capacity_ledger.h](../src/host/worker_capacity_ledger.h)、[worker_capacity_ledger.cc](../src/host/worker_capacity_ledger.cc)、[generation_pool.h](../src/host/generation_pool.h)、[generation_pool.cc](../src/host/generation_pool.cc)、[managed_listener.cc](../src/host/managed_listener.cc)、[worker_executor.cc](../src/host/worker_executor.cc) |
| Stage 3 | `e44d6ad` | 全局用户态内存水位二阶段公平 LRU 淘汰、多租户配额与管理面租户字段 | [memory_sampler.h](../src/host/memory_sampler.h)、[memory_sampler.cc](../src/host/memory_sampler.cc)、[eviction_coordinator.h](../src/host/eviction_coordinator.h)、[eviction_coordinator.cc](../src/host/eviction_coordinator.cc)、[process_snapshot.cc](../src/host/process_snapshot.cc)、[active_state.cc](../src/host/active_state.cc)、[admin_api.cc](../src/host/admin_api.cc)、[managed_admin_backend.cc](../src/host/managed_admin_backend.cc)、[managed_host.cc](../src/host/managed_host.cc)、[metrics.cc](../src/host/metrics.cc) |

### 各阶段测试文件映射

- Stage 1：[test_host_config.cc](../tests/test_host_config.cc)、[test_host_config_model.cc](../tests/test_host_config_model.cc)、[test_host_policy_compiler.cc](../tests/test_host_policy_compiler.cc)、[test_host_cli_unknown_arguments.mjs](../tests/test_host_cli_unknown_arguments.mjs)
- Stage 2：[test_worker_capacity_ledger.cc](../tests/test_worker_capacity_ledger.cc)、[test_host_elastic_pool.cc](../tests/test_host_elastic_pool.cc)
- Stage 3：[test_host_eviction_coordinator.cc](../tests/test_host_eviction_coordinator.cc)、[test_host_active_state.cc](../tests/test_host_active_state.cc)、[test_host_admin_api.cc](../tests/test_host_admin_api.cc)

---

## 2. 硬性架构红线（违反任一条即为 `P0` 阻断）

1. **冻结契约与公开 C ABI 零改动**：
   核验以下命令输出必须为空，确认公开 C ABI v7、线缆协议、工作树锁与已发布 `v0.3.1-beta.1` 元数据完全未被改动：
   ```sh
   git diff a7fd47e..e44d6ad -- \
     include/capsid/runtime.h \
     include/capsid/runtime.hpp \
     src/protocol.h \
     src/memory_metrics.cc \
     tests/fixtures/websocket-interop-lock.json \
     docs/current-status.json \
     docs/data \
     README.md
   ```
   同时核验 [config.cc](../src/host/config.cc) 与 [net_target.cc](../src/net_target.cc) 对既有 `capsid/host-v1`、`capsid/app-v1`、`capsid/binding-v1` 及 `parse_fetch_target_text` 的兼容与拒绝语义保持不变。
2. **禁止内核态软水位写入**：
   全局内存水位淘汰必须保持纯用户态只读采样 + 驱逐空闲 Worker 进程。**严禁**写入 Linux cgroup `memory.high`，**严禁**把应用软内存或整机预算写入 `RLIMIT_AS`。
3. **描述符与生产沙箱边界**：
   `capsid_worker_fd` 不得泄露到 `worker_event_source.{h,cc}` 与 `worker_supervisor.cc` 之外；[local_dev_host.cc](../src/host/local_dev_host.cc) 中供本地测试/调试使用的环境变量（`CAPSID_LOCAL_SOURCE_NAME`、`CAPSID_HOST_IPC_METRICS`、`CAPSID_INITIAL_STREAM_WINDOW`）不得削弱 `--mode managed` 生产严格沙箱行为。
4. **100% 配置字段消费（禁止死配置）**：
   [config.cc](../src/host/config.cc) 为 `host-v2` 与 `app-v2` 定义的每个 Schema 成员，必须在 [host_config_model.cc](../src/host/host_config_model.cc) 与 [policy_compiler.cc](../src/host/policy_compiler.cc) 中实际消费，并在运行时（池行为、淘汰、限制、监听器或路由）真正生效，尤其关注 `kCapacityMemoryV2Schema`、`kTenancyV2Schema` 与 `app-v1`/`app-v2` 禁止 `tenant` 的约束。

---

## 3. 对抗式深挖审校维度

### 3.1 并发模型、跨模块锁序与生命周期安全

重点排查 [generation_pool.cc](../src/host/generation_pool.cc)、[eviction_coordinator.cc](../src/host/eviction_coordinator.cc)、[worker_capacity_ledger.cc](../src/host/worker_capacity_ledger.cc)、[managed_listener.cc](../src/host/managed_listener.cc)：

- **跨锁顺序与死锁环路**：
  梳理 `GlobalEvictionCoordinator`（`eviction_mutex_` / `registry_mutex_` / `monitor_mutex_`）、`GenerationPool::mutex_`、`WorkerCapacityLedger::mutex_` 与 `ManagedListener` IO 线程（含 `WorkerEventMailbox`）之间的加锁顺序。核查 `GenerationPool` 在触发外部钩子（`acquire_worker_slot`、`release_elastic_slot`、`on_worker_ready_`、`on_scale_out_failed_`）及执行进程回收/join 时是否严格释放自身 `mutex_`，是否存在 `coordinator -> pool` 与 `pool -> coordinator` 对锁风险。
- **Victim 淘汰与请求/WebSocket 派发之间的 TOCTOU**：
  当 `evict_idle_worker()`（由空闲 Reaper 或 `GlobalEvictionCoordinator` 调用）选中某个 `ready` 槽位执行驱逐时，若 `ManagedListener` 的 IO 线程并发调用 `GenerationPool::pick_worker()` 拿到同一个 `WorkerExecutor*` 准备调用 `submit()` 或完成 WebSocket 101 升级：
  - `pick_worker()` 的持锁判断、活跃通道计数（`active_channels`）与 HTTP `inflight` 递增是否构成原子同步边界？
  - 刚从 `pick_worker()` 返回但尚未计入 `inflight()` 的空窗期内，若该槽位被 `evict_idle_worker()` 发送关停帧并移入 `retired_`，该请求会如何退避或失败（能否由协调器安全拒绝/重派而不丢请求、不强杀在途连接）？
- **Scale-to-Zero（`minReady = 0`）队列唤醒与失活收敛**：
  追踪 `ListenerSession::handle_request` -> `pool->request_scale_out()` -> `ManagedListenerImpl::enqueue_or_reject` -> `pump_pool_queue` / `fail_pool_queue_unavailable` / `on_queue_timer` 路径：
  - 若 `request_scale_out()` 返回 `kSpawnInFlight`，但在 `enqueue_or_reject()` 之前后台 Worker 已完成就绪或立即失败，挂起请求是否会被后续回调可靠唤醒？
  - 当连接中途断开（`ListenerSession::close()` -> `cancel_queued_for_session`）、路由切换到新池或旧池进入 `kDraining` 时，旧池对应的 `pool_queues_` 是否100%清空与释放？
  - `ManagedListenerImpl::on_queue_timer` 与 `pump_pool_queue` 的超时路径是否均通过 `expire_front_queued` 准确累加 `capsid_pool_rejections_total{..., reason="queue_timeout"}`。
- **异步销毁与停机收敛**：
  检查 `GenerationPool::stop_and_join()` / `~GenerationPool()` 对 `retired_` 异步回收线程、后台扩容线程（`replacements_in_flight_`）以及 `GlobalEvictionCoordinator::stop_and_join()` 对 `monitor_thread_` 的汇合时序，确认不存在析构顺序导致的悬垂指针或孤儿 Worker 进程。

### 3.2 容量账本守恒（`WorkerCapacityLedger` 与租户配额）

重点排查 [worker_capacity_ledger.h](../src/host/worker_capacity_ledger.h)、[worker_capacity_ledger.cc](../src/host/worker_capacity_ledger.cc)、[managed_host.cc](../src/host/managed_host.cc)、[main.cc](../src/host/main.cc)、[eviction_coordinator.cc](../src/host/eviction_coordinator.cc)：

- **稳态（`steady`） / 弹性（`elastic`） / 过渡峰值（`surge`）守恒**：
  逐条追踪以下场景的 `reserve_*`、`commit_*`、`abort_reserve`、`begin_retire`、`release_drained`、`release_active`、`try_acquire_elastic`、`release_elastic`：
  1. 初次部署成功与预热失败回滚；
  2. 同版本或新版本替换部署（`reserve_replace` -> `commit_replace`）；
  3. 托管进程重启恢复 `managed_recover` 后，在 `adopt_recovered` 期间按当前状态重新占账与后续退役释放；
  4. 崩溃重启（`install_replacement` / `handle_executor_exit`）及超预算进入 Quarantine 时，稳态槽位与额外弹性槽位（`elastic_held`）是否全部退账不泄漏；
  5. `tenant_quota_swap` 驱动 `evict_idle_worker` 成功驱逐兄弟应用 1 个空闲弹性 Worker 并调用 `release_elastic` 后，若紧接申请 `try_acquire_elastic` 被并发池抢占或后续 `spawn` 失败，已释放槽位是否严格归零守恒。

### 3.3 二阶段公平内存淘汰与只读采样准确性

重点排查 [memory_sampler.cc](../src/host/memory_sampler.cc)、[process_snapshot.cc](../src/host/process_snapshot.cc)、[eviction_coordinator.cc](../src/host/eviction_coordinator.cc)：

- **只读采样器健壮性**：
  核查 Linux cgroup v2 `memory.current` 读取与多进程工作集回退链（`read_process_footprint_bytes`：`/proc/<pid>/smaps_rollup` PSS -> `statm`/RSS，Windows 工作集回调/测试注入）在进程瞬时退出（`ESRCH`/`ENOENT`）、权限受限、缓存 TTL 边界（`sample_interval_ms`）及整数累加下的防溢出处理。
- **迟滞带与公平份额数学闭合性**：
  - 高水位触发（`total_bytes >= limit * highWatermark / 100`）后，是否持续淘汰至 `total_bytes <= limit * lowWatermark / 100`（或无合规候选者时安全终止），确认在无候选者场景不会自旋死循环。
  - Phase 1 动态公平份额公式 `soft_share = max(limit / N_active_tenants, minSoftMemory)`：当租户总内存超过 `soft_share` 时是否优先淘汰超额比例最高租户下最久未用的合法空闲弹性 Worker；当没有任何超额租户持有可淘汰空闲实例时，是否正确落入 Phase 2（`global_lru`）按全局最旧空闲时间淘汰。
  - 3 道硬保护线核验：任何阶段均不得淘汰 `inflight > 0` 或 `active_channels > 0` 的忙碌实例、不得将池活实例数降至 `minReady` 以下（`live > minReady` 且弹性持槽约束一致）、不得淘汰存活时间小于 `capacity.memory.minResidency` 的年轻实例。
  - `hardMemory` 与 `maxWorkers` 约束核验：静态部署/恢复期（`check_deploy_quota`）与运行期扩容（`acquire_worker_slot`）的协同及失败指标理由标签是否一致（`tenant_max_workers`、`tenant_hard_memory`、`host_memory_budget`、`host_capacity_exhausted`）。

### 3.4 管理面、持久化、指标与本地同构安全

重点排查 [admin_api.cc](../src/host/admin_api.cc)、[managed_admin_backend.cc](../src/host/managed_admin_backend.cc)、[active_state.cc](../src/host/active_state.cc)、[metrics.cc](../src/host/metrics.cc)、[local_dev_host.cc](../src/host/local_dev_host.cc)：

- `tenant` ID 规则校验（字符集、长度上限）在 `host.json#tenancy.overrides`、Admin API `POST /v1/deploy`、`active.json` 解析及 Prometheus 渲染（`capsid_pool_evictions_total{app,tenant,reason}` / `capsid_pool_rejections_total{app,tenant,reason}`）之间是否完全一致、无标签注入风险。
- 旧版未带 `"tenant"` 字段的 `active.json` 升级恢复、等价回退（`tenant == app` 时省略序列化 `"tenant"`）以及退役 tombstone / quarantined 状态往返读写是否严格保持兼容性与幂等性。
- [local_dev_host.cc](../src/host/local_dev_host.cc) 在读取 `--app-dir` 内 `capsid.json`、`entry` 文件及 `host.json` 时是否严防 `..` 路径穿越、符号链接越界、FIFO/设备文件阻塞或超大文件导致 OOM。

---

## 4. 已完成验证基线与复现命令

Commit `e44d6ad` 合入并验证的基线如下：

- **格式化与文档覆盖率门禁**：
  - `tools/format.py --check`：`{"needsFormatting": 0}`（检查 C++ 327、JavaScript 287、Python 63、Data 12）。
  - `node tests/audit-current-docs.mjs`：`current docs: 59 documents, 31 archived snapshots hash-checked, 12 modules verified`。
- **WSL2 Linux 全量测试（`test_wsl.sh all`，共 1017 项）**：
  - **1014 项通过、0 项失败**（`100% tests passed out of 1017`，包含 `host_eviction_*` 4/4、`host_routing_snapshot_contract`、`websocket_async_callbacks_*`、`websocket_budget_*` 与 `worker_build_identity_matrix`）。
  - 3 项环境预期跳过：`host_managed_executable_active_state_foreign_owner`、`worker_sandbox_cgroup_v2`、`worker_sandbox_network_namespace`。
- **Windows 快速测试（`test_win.bat fast`，共 576 项）**：
  - **567 项通过**、8 项环境预期跳过（`host_single_worker_ab_emits_complete_evidence` 与 7 项 `worker_binding_sandbox_*`）。
  - `#546 worker_build_identity_matrix` 在 Windows 本地仅因工作区根目录历史遗留的未跟踪目录 `.playwright-mcp/` 触发 `git status --porcelain` 非空前置校验而拦截（同一代码在干净 WSL2 源树已通过 `#978 worker_build_identity_matrix`）。

### 常用局部复现命令

```sh
# 1. 文档与覆盖率清单审计
node tests/audit-current-docs.mjs

# 2. Linux / WSL2 定向测试（Host v2 / 弹性池 / 水位淘汰 / 路由快照）
ctest --test-dir /home/eroszhao/capsid-build-v032 --output-on-failure -j 8 \
  -R '^(host_config.*|host_policy_compiler|host_active_state|host_admin_api.*|host_worker_capacity_ledger.*|host_elastic_pool_.*|host_eviction_.*|host_routing_snapshot_contract)$'

# 3. 验证公开 ABI 与冻结文件零改动
git diff --stat a7fd47e..e44d6ad -- \
  include/capsid/runtime.h include/capsid/runtime.hpp src/protocol.h \
  src/memory_metrics.cc tests/fixtures/websocket-interop-lock.json \
  docs/current-status.json docs/data README.md
```

---

## 5. 推荐审校输出结构

请独立审校员按以下格式输出审校结论：

1. **总体判定**：是否满足合并主线或发布 `v0.3.2-beta` 的质量要求。
2. **发现问题清单**（按 `P0 阻断` / `P1 严重` / `P2 一般` / `P3 建议` 分级；对于核查后未发现问题的模块，也请明确写明已验证的不变量与代码路径）：
   - **位置**：`文件路径:行号`（及对应 commit SHA）
   - **根因与触发时序**：具体线程 / 请求 / 配置交错推导
   - **影响范围**：崩溃 / 死锁 / 账本泄漏 / 契约偏离 / 指标失真 / 安全边界削弱等
   - **修复建议**：最小且保持设计正交性的修正方案
3. **测试盲区评估**：指出 [test_host_elastic_pool.cc](../tests/test_host_elastic_pool.cc) 与 [test_host_eviction_coordinator.cc](../tests/test_host_eviction_coordinator.cc) 尚未覆盖到的高风险并发边界或异常注入路径。
