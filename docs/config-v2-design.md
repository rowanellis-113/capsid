# Capsid 配置 v2 正交化重构与 v0.3.2 多租户弹性池架构方案

> **状态：架构与配置演进设计方案（尚未在 v0.3.1-beta 中实现）。**
> 本文档完整沉淀并收敛了围绕 **全量配置正交性重构（Config v2）**、**本地开发与 `managed` 生产产物同构** 以及 **v0.3.2 多租户弹性 Worker 池与全局内存预算 LRU 淘汰** 的全部架构讨论与设计决策。现行已交付行为请以 [Host 配置参考](host-config.md)、[capsid.json 指南](capsid-json.md) 与 [当前状态](current-status.md) 为准。

---

## 目录

- **第一部分（Phase 1 基线重构）：基于现有能力的配置 v2 精简与正交化**
  - [1. 现有配置与 CLI 的全局痛点诊断](#1-现有配置与-cli-的全局痛点诊断)
  - [2. 配置 v2 的核心正交准则与字段瘦身](#2-配置-v2-的核心正交准则与字段瘦身)
  - [3. 配置 v2 最简与最全实例（仅含现有功能）](#3-配置-v2-最简与最全实例仅含现有功能)
  - [4. 容量层级辨析与全字段速查字典](#4-容量层级辨析与全字段速查字典)
  - [5. 本地开发模式去冗与 `managed` 产物同构设计](#5-本地开发模式去冗与-managed-产物同构设计)
- **第二部分（Phase 2 弹性演进）：多租户弹性池与全局内存预算 LRU 方案（v0.3.2+）**
  - [6. 与 `workerd` 的架构差异对标](#6-与-workerd-的架构差异对标)
  - [7. 多租户公平性与二阶段内存 LRU 淘汰机制](#7-多租户公平性与二阶段内存-lru-淘汰机制)
  - [8. 合理性审视：6 个关键工程隐患与修正闭环](#8-合理性审视6-个关键工程隐患与修正闭环)
  - [9. 易用性审视：在配置 v2 底座上的极简增量扩展](#9-易用性审视在配置-v2-底座上的极简增量扩展)
  - [10. 兼容性保障与分阶段落地路线图](#10-兼容性保障与分阶段落地路线图)

---

# 第一部分（Phase 1 基线重构）：基于现有能力的配置 v2 精简与正交化

> **Phase 1 边界原则**：第一部分严格限定在 Capsid **当前已交付功能范围**（固定大小 Worker 池、Bindings v1、FetchRPC v4 长连接通道、准入排队、慢连接保护、崩溃自愈与严格沙箱），**不掺杂任何未实现的动态扩缩容或多租户参数**，先把配置底座彻底理顺、做薄、做正交。

## 1. 现有配置与 CLI 的全局痛点诊断

对照当前 `src/host/config.cc`、`src/host/host_config_model.cc`、`src/host/policy_compiler.cc` 与 `src/host/main.cc` 源码逐项排查，现行 v1 / 过渡态 v2 存在 **6 类结构性问题**：

| 问题类别 | 源码中的真实表现 | 对运维与开发者的直接痛点 |
| --- | --- | --- |
| **1. 静默死字段（写了完全不生效）** | ① **6 个空壳字段**：`worker.pidsMax`、`capacity.startupsConcurrent`、`capacity.workerMemoryCommitTotal`、`capacity.queuedRequestsTotal`、`capacity.queuedHeaderBytesTotal` 只解析零消费；`recovery.restartBackoff.stableReset` 在 Schema 可写但 `host_config_model.cc` 直接硬编码 `60000` 未读 JSON。<br>② **`defaults` 假对称**：`host.json#defaults` 暴露 13 个字段，实际**仅 `defaults.request.timeout` 1 个字段接入策略编译器**，其余 12 个字段完全不生效。 | 严重违背 Fail-Closed 预期：用户配了 `defaults.pool.queueRequests` 或 `defaults.worker.memoryMax` 以为全局生效，实际静默失效 |
| **2. C++ 内部管道参数过度外溢** | WebSocket 暴露了 7 个底层环形缓冲/握手常量（`reassemblyBudgetBytes`、`sendBudgetBytes`、`perChannelQueueBytes`、`channelQueueBytesTotal`、`pendingDialsTotal`、`dialTimeoutMs`、`closeTimeoutMs`）；蓝绿发布暴露 `activationSurgeWorkers`（默认 `0` 导致容量卡满时无法热更新）；自愈暴露 `restartBackoff.*` 与 `replacementsConcurrentPerApp`；`admin.mode` 只允许填 `"0600"` | 大量生僻内部状态机词汇淹没了真正需要关注的业务配置 |
| **3. 单位与类型分裂** | **时间**：`timeout` / `queueTimeout` / `listeners[].limits.streamIdleTimeout` 用字符串 `"60s"`，紧挨着的 `request.{streamIdleTimeoutMs, writeTimeoutMs}` 与 `channels.*TimeoutMs` 却强制填裸整数 `60000`；**大小**：大部分用 `"16MiB"`，但 `channels.*Bytes` 强制用裸整数 `16777216`，而 `headerBytes` 名字叫 `*Bytes` 却要求填字符串 `"32KiB"`；**出站地址**：App `fetch.allow` 与 Binding `net.allow` 语法不兼容 | 同一份配置文件里相同物理量有两套类型与命名规则，极易写错拒载 |
| **4. 版本表复制漏项** | `kHostMembers`（`capsid/host-v1`）有顶层 `channels`，而 `kHostV2Members`（`capsid/host-v2`）新增 `bindingsRoot` 时**漏抄了 `channels`** | 同一份 `host.json` 无法同时开启 Bindings 与协议层长连接限额 |
| **5. 固定池冗余与跨平台锁死** | 当前仅有固定池，但 `capsid.json#pool` 强制要求同时填写 `minReady` 和 `maxWorkers` 且校验相等；`capsid.json` 暴露仅 Linux 支持的 `worker.processAddressSpace`，搬到 macOS 直接报错拒载 | 1 个自由度被迫写 2 个相等字段；同一份 `capsid.json` 无法在 Linux 与 macOS/Windows 间通用 |
| **6. 本地 CLI 高度冗余且与 `managed` 脱节** | 本地跑 Hello World 强制手敲 12 个参数；`single-worker` 与 `static-pool` 人为拆成两个模式；本地单应用强制要求 `/@capsid/<app>/` 路由前缀；36 个 CLI 参数把 JSON 重抄一遍且忽略 `capsid.json#pool` | 上手门槛极高，且若依赖 CLI 参数覆盖配置，上线 `managed` 模式时会出现产物不兼容 |

---

## 2. 配置 v2 的核心正交准则与字段瘦身

### 2.1 四层正交职责切面（每件事只在唯一一处表达）

```text
┌────────────────────────────────────────────────────────────────────────────┐
│ [层 1] Host 环境与入口: paths | admin | isolation | trustedBytecodeKeys    │
│                         listeners | recovery                               │
├────────────────────────────────────────────────────────────────────────────┤
│ [层 2] Host 整机总容量: capacity.{workersTotal, channelsTotal,             │
│                                   maxChannelMessageSize, channelIdleTimeout}│
├────────────────────────────────────────────────────────────────────────────┤
│ [层 3] Host 单应用夹板: permissions (能力白名单交集上限)                   │
│                         defaults.{worker, request, pool} (100% 真默认值)   │
│                         maximums.{worker, request, pool} (完全同构硬上限)  │
├────────────────────────────────────────────────────────────────────────────┤
│ [层 4] App 业务契约:    entry | permissions | bindings | healthCheck       │
│        (capsid.json)    worker | request | pool (可选，自动继承 defaults)  │
└────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 激进瘦身清单：只留业务决策旋钮，内部细节收归引擎自动推导

| 旧版冗余/生僻字段 | 配置 v2 处理决策 |
| --- | --- |
| 6 个未接线死字段（`pidsMax`, `workerMemoryCommitTotal`, `queuedRequestsTotal`, `queuedHeaderBytesTotal`, `startupsConcurrent`, `stableReset`） | **直接删除**，配合 Schema 全字段消费单测，永久杜绝空壳字段 |
| `admin.mode`（唯一合法值 `"0600"`） | **直接删除**，Host 固定强制 `0600` 权限与 `SO_PEERCRED` 同 UID 校验 |
| 顶层 4 个零散路径（`applicationsRoot`, `stateRoot`, `secretRootTemplate`, `bindingsRoot`） | 收拢为 `paths.{applications, state, secrets, bindings}` |
| `listeners[].{publicScheme, publicAuthority}` 两个必填项 | 合并为 1 个可选字段 `publicOrigin`（如 `"https://apps.example.com"`，省略时默认取 `"http://<tcp>"`） |
| `capacity.activationSurgeWorkers` | **收归内部默认**，蓝绿热更新期间自动预留 `max(2, workersTotal * 25%)` 的临时切换重叠槽 |
| WebSocket 7 个底层流控缓冲/握手字段（`reassemblyBudget`, `sendBudget`, `perChannelQueue`, `channelQueue`, `pendingDials`, `dialTimeout`, `closeTimeout`） | **收归内部协议默认比例推导**，对外仅保留 `channelsTotal`、`maxChannelMessageSize`、`channelIdleTimeout` 3 个直观参数（同时彻底修复 `host-v2` 漏项） |
| `recovery.restartBackoff.*` 与 `replacementsConcurrentPerApp` | **收归内部标准指数退避**（`100ms..10s`、`10%` 抖动、`60s` 稳定重置、单应用串行替换），仅保留直观的 `crashBudget` 与 `healthCheck` |
| `pool.queueHeaderBytes` | **删除**，排队请求头内存天然受 `listeners[].limits.headerSize × pool.queueRequests` 严格约束 |
| `request.{maxStreamingInflightPerWorker, streamIdleTimeoutMs, writeTimeoutMs}` | 慢连接与流空闲保护统一归口至入口层 `listeners[].limits`；应用层 `request` 只保留最核心的 `timeout` 与 `maxInflight` |
| `worker.processAddressSpace` | 从跨平台的 `capsid.json` 移除，由 Linux 严格沙箱根据 `memoryMax` 自动施加地址空间保护 |
| `pool.{minReady, maxWorkers}` 强制相等必填 | 在 Phase 1 固定池基线中简化为可选单一字段 `pool.workers`（省略即继承 `defaults.pool.workers ?? 1`） |

### 2.3 全域统一语法与 100% 同构继承代数

1. **物理量语法唯一化**：
   - **时间（Duration）**：一律采用 `"100ms"` / `"5s"` / `"1m"` 字符串，废除所有 `*Ms` 裸整数；
   - **字节大小（Size）**：一律采用 `"32KiB"` / `"256MiB"` / `"2GiB"` 字符串，废除所有 `*Bytes` 裸整数；
   - **出站网络目标**：`permissions.fetchTargets`、App `permissions.fetch.allow` 与 Binding `permissions.net.allow` 共用同一套 `<host>[:<p1>[,<p2>...]]` 解析器（支持精确域名、`*.` 前缀通配、IPv4/IPv6 字面量与 CIDR）。
2. **`defaults` / `maximums` / `capsid.json` 完全同构求值**：
   三处的 `{ worker, request, pool }` 拥有**完全一致的 8 个叶子字段**，统一执行确定性求值：
   $$\text{effective}(f) = \text{app}(f) \;\text{??}\; \text{host.defaults}(f) \;\text{??}\; \text{builtin}(f)$$
   启动期强制校验 $\text{host.defaults}(f) \le \text{host.maximums}(f)$；部署期若显式 `app(f)` 超出 `host.maximums(f)` 则 Fail-Closed 拒绝部署（未显式声明时内置默认值自动向下钳制到 `host.maximums(f)`）。

---

## 3. 配置 v2 最简与最全实例（仅含现有功能）

### 3.1 最简配置（Minimal）

#### 最简 `host.json`
```json
{
  "apiVersion": "capsid/host-v2",
  "paths": {
    "applications": "/srv/capsid/applications",
    "state": "/srv/capsid/state",
    "secrets": "/srv/capsid/secrets/{application}"
  },
  "admin": {
    "unix": "/run/capsid/admin.sock"
  },
  "listeners": [
    {
      "name": "public",
      "tcp": "0.0.0.0:8080",
      "routing": { "mode": "path" }
    }
  ]
}
```

#### 最简 `capsid.json`
```json
{
  "apiVersion": "capsid/app-v2"
}
```

### 3.2 最全配置（Maximal — 打开每一个合法字段）

#### 最全 `host.json`
```json
{
  "apiVersion": "capsid/host-v2",

  "paths": {
    "applications": "/srv/capsid/applications",
    "state": "/srv/capsid/state",
    "secrets": "/srv/capsid/secrets/{application}",
    "bindings": "/etc/capsid/bindings"
  },

  "admin": {
    "unix": "/run/capsid/admin.sock"
  },

  "isolation": {
    "mode": "strict",
    "required": ["cgroup-v2"],
    "cgroupRoot": "/sys/fs/cgroup/capsid"
  },

  "trustedBytecodeKeys": {
    "2026-11": "/etc/capsid/keys/ed25519-2026-11.pub"
  },

  "listeners": [
    {
      "name": "public",
      "tcp": "0.0.0.0:8080",
      "publicOrigin": "https://apps.example.com",
      "trusted": false,
      "routing": {
        "mode": "subdomain",
        "suffix": ".apps.example.com"
      },
      "limits": {
        "connections": 512,
        "headerSize": "32KiB",
        "headerTimeout": "5s",
        "bodyIdleTimeout": "30s",
        "streamIdleTimeout": "60s",
        "writeTimeout": "10s",
        "channels": 128
      },
      "cors": {
        "allowedOrigins": ["https://dev.example.com"],
        "allowedMethods": ["GET", "POST", "OPTIONS"],
        "allowedHeaders": ["content-type", "capsid-app", "access-token"],
        "maxAge": "86400s"
      }
    }
  ],

  "permissions": {
    "modules": ["capsid:env", "capsid:fs", "capsid:storage", "capsid:stdio"],
    "environmentNames": ["APP_MODE", "DATABASE_*"],
    "fsReadRoots": ["/srv/capsid/config"],
    "fetchTargets": ["api.example.com:443", "metrics.example.com:443"],
    "storageNamespaces": ["session"],
    "stdioStreams": ["stdout", "stderr"]
  },

  "defaults": {
    "worker": {
      "memoryMax": "256MiB",
      "jsHeap": "64MiB",
      "fileDescriptors": 64
    },
    "request": {
      "timeout": "5s",
      "maxInflight": 64
    },
    "pool": {
      "workers": 2,
      "queueRequests": 256,
      "queueTimeout": "10s"
    }
  },

  "maximums": {
    "worker": {
      "memoryMax": "512MiB",
      "jsHeap": "256MiB",
      "fileDescriptors": 256
    },
    "request": {
      "timeout": "30s",
      "maxInflight": 128
    },
    "pool": {
      "workers": 16,
      "queueRequests": 1024,
      "queueTimeout": "30s"
    }
  },

  "capacity": {
    "workersTotal": 64,
    "channelsTotal": 1024,
    "maxChannelMessageSize": "1MiB",
    "channelIdleTimeout": "60s"
  },

  "recovery": {
    "crashBudget": {
      "maxEvents": 5,
      "window": "60s"
    },
    "healthCheck": {
      "interval": "30s",
      "failures": 3
    }
  }
}
```

#### 最全 `capsid.json`
```json
{
  "apiVersion": "capsid/app-v2",
  "entry": "bundle.mjs",

  "permissions": {
    "modules": ["capsid:env", "capsid:fs", "capsid:storage", "capsid:stdio"],
    "env": {
      "APP_MODE": { "value": "production" },
      "DATABASE_URL": { "valueFrom": "db-url" }
    },
    "fs": {
      "read": {
        "allow": ["/srv/capsid/config"],
        "deny": ["/srv/capsid/config/private.json"]
      }
    },
    "fetch": {
      "allow": ["api.example.com:443", "metrics.example.com:443"]
    },
    "storage": {
      "namespaces": ["session"]
    },
    "stdio": ["stdout", "stderr"]
  },

  "bindings": {
    "redis": {
      "permissions": {
        "net": { "allow": ["10.0.1.15:6379"] }
      },
      "config": {
        "keyPrefix": "orders:"
      },
      "secrets": {
        "password": { "valueFrom": "redis-password" }
      }
    }
  },

  "worker": {
    "memoryMax": "256MiB",
    "jsHeap": "64MiB",
    "fileDescriptors": 64
  },

  "request": {
    "timeout": "5s",
    "maxInflight": 64
  },

  "pool": {
    "workers": 2,
    "queueRequests": 256,
    "queueTimeout": "10s"
  },

  "healthCheck": {
    "path": "/health",
    "timeout": "2s"
  }
}
```

---

## 4. 容量层级辨析与全字段速查字典

### 4.1 关键辨析：`capacity.workersTotal` 与 `pool.workers` 的区别

在 `managed` 模式下，**一台 `capsid-host` 同时托管多个不同应用**，因此进程数配置分为清晰的“单应用级”与“整机级”两层账本：

```text
整台机器 capacity.workersTotal = 64  (全机总进程池天花板，防宿主机过载)
 ├── App 1 (orders):  capsid.json 配了 pool.workers = 2
 ├── App 2 (payment): capsid.json 省略 pool → 自动继承 host defaults.pool.workers = 2
 ├── App 3 (search):  capsid.json 配了 pool.workers = 12 (受 host maximums.pool.workers = 16 限制)
 └── 剩余可用槽位:    64 - (2 + 2 + 12) = 48 个，供其他应用部署使用
```

| 字段位置 | 作用范围 | 核心职责 |
| --- | --- | --- |
| `capsid.json` → `pool.workers` | **当前单个应用** | 业务方声明该应用需要开启几个 Worker 进程 |
| `host.json` → `defaults.pool.workers` | **单应用默认值** | 当应用省略 `pool.workers` 时默认分配几个进程 |
| `host.json` → `maximums.pool.workers` | **单应用硬上限** | 防止某**一个**应用填过大数值独占整台机器，饿死其他应用 |
| `host.json` → `capacity.workersTotal` | **整机所有应用总和** | 锁死整台宿主机上所有已部署应用 `pool.workers` 的累加总上限（超出时拒绝新部署） |

### 4.2 `host.json` v2 全模块速查字典

| 模块 | 字段 | 必填 | 默认值 | 含义说明 |
| --- | --- | --- | --- | --- |
| `paths` | `applications` / `state` / `secrets` / `bindings` | 前三项必填 | `bindings`: 空 | 应用包根目录、状态持久化目录、含 `{application}` 占位符的密钥目录模板、可选 Binding 插件目录 |
| `admin` | `unix` | ✓ | — | 本地管理 Unix Socket 路径（内置强制 `0600` 权限与同 UID 校验） |
| `isolation` | `mode` / `required` / `cgroupRoot` | | `mode: "strict"` | Linux 生产严格沙箱模式、必选内核特性列表与委托 cgroup 根目录 |
| `trustedBytecodeKeys` | `<keyId>: <path>` | | `{}` | 预编译字节码验签公钥 ID 到 Ed25519 公钥路径的映射 |
| `listeners[]` | `name` / `tcp` / `publicOrigin` / `trusted` / `routing` / `limits` / `cors` | `name`, `tcp`, `routing.mode` | `publicOrigin: "http://<tcp>"`, `trusted: false` | 流量入口、外部反向代理 Origin、路由方式（`path` \| `subdomain` \| `header`）、慢连接/并发连接保护与网关级 CORS |
| `permissions` | `modules` / `environmentNames` / `fsReadRoots` / `fetchTargets` / `storageNamespaces` / `stdioStreams` | | 全空（Deny-All） | 整机允许应用申请的权限总白名单上限（与 `capsid.json#permissions` 求交集） |
| `defaults` / `maximums` | `worker.{memoryMax, jsHeap, fileDescriptors}`、`request.{timeout, maxInflight}`、`pool.{workers, queueRequests, queueTimeout}` | | 内置默认值 / `0`（不限） | 单应用的**真默认值**与**硬上限** |
| `capacity` | `workersTotal` / `channelsTotal` / `maxChannelMessageSize` / `channelIdleTimeout` | | `1` / `1024` / `"1MiB"` / `"60s"` | 整机最大常驻 Worker 总数与整机 WebSocket 连接及单消息限制 |
| `recovery` | `crashBudget.{maxEvents, window}` / `healthCheck.{interval, failures}` | | `5次/60s`、`30s/3次` | 崩溃隔离阈值（防死循环重启）与主动健康检查探活周期 |

---

## 5. 本地开发模式去冗与 `managed` 产物同构设计

### 5.1 核心设计铁律：本地只是 `managed` 产物的轻量宿主运行壳（Host Stub）

本地模式的唯一职责是**在开发机（Linux / macOS / Windows）上直接挂载并运行符合 `managed` 规范的应用版本产物**，绝不能演变成绕过 `capsid.json` 的第二套配置系统，否则必然导致“本地靠命令行开关跑通，上线 `managed` 因缺配置而部署失败”。

1. **产物目录 100% 同构**：
   本地开发目录（如 `./dist/`）与线上 `<paths.applications>/<app>/<version>/` 拥有**完全一致的文件结构与校验规则**：
   ```text
   ./dist/
     ├── capsid.json     (capsid/app-v2 唯一应用配置权威)
     └── bundle.mjs      (自包含 ESM 产物，或由 capsid.json#entry 指定)
   ```
   应用自身的一切行为（入口、权限、Bindings、内存、超时、并发、`pool.workers`、健康检查）**100% 只从该目录的 `capsid.json` 读取**，废除原先在 CLI 上重复抄写一遍的 36 个应用级覆盖参数。

2. **合并 `single-worker` 与 `static-pool`，消除 12 参数样板**：
   - `single-worker` 只是 `workers == 1` 的特例，不再拆分为两个模式；
   - 默认自动查找与 `capsid-host` 同目录的同构建 `capsid-worker` 二进制；
   - 默认直接挂载在根路径 `/`（无需传 `--application` 与 `/@capsid/<app>/` 前缀——这与线上 `managed` 网关剥离路由前缀后 Worker 内部看到的 `request.url` 路径完全一致！）；
   - 本地 CLI 仅保留模拟线上 `host.json` 外部环境的 **最小宿主桩参数**：

   | 本地 CLI 参数 | 默认值 | 对应线上 `host.json` 的职责 |
   | --- | --- | --- |
   | `--app-dir <dir>` | `.`（当前目录） | 指向符合 `managed` 规范的版本产物目录 |
   | `--listen <ip:port>` | `127.0.0.1:8080` | 模拟 `listeners[].tcp` 本地监听地址 |
   | `--secrets-dir <dir>` | 空 | 模拟 `paths.secrets` 单应用密钥目录，本地联调 `valueFrom` |
   | `--bindings-dir <dir>` | 空 | 模拟 `paths.bindings` 插件目录，本地联调 `bindings` |
   | `--workers <n>` | 读取 `capsid.json#pool.workers`（默认 `1`） | 仅本地压测时临时覆盖并发进程数 |
   | `--strict-sandbox` | Linux 可选 `on`/`off`，macOS/Windows 自动 `off` | Linux 本地预验生产严格沙箱 |
   | `--ready-fd` / `--stop-fd` | 可选（交互终端无需填写） | 仅自动化测试框架或父进程守护器使用 |

   **优化前后本地开发命令对比**：
   ```sh
   # 旧版（12 个必填参数 + 必须带 /@capsid/hello/ 前缀）：
   capsid-host --mode single-worker --worker "$CAPSID_BIN/capsid-worker" \
     --source-bundle ./bundle.mjs --source-name "file://$PWD/bundle.mjs" \
     --application hello --listen 127.0.0.1:8080 --routing path \
     --public-scheme http --public-authority 127.0.0.1:8080 \
     --request-timeout-ms 5000 --strict-sandbox off --ready-fd 1 --capsid-json ./capsid.json

   # 新版 v2（在标准产物目录直接一键运行，跑通即可原样上线 managed）：
   capsid-host --app-dir ./dist
   ```

---

# 第二部分（Phase 2 弹性演进）：多租户弹性池与全局内存预算 LRU 方案（v0.3.2+）

> **设计衔接**：在第一部分立住干净、正交的配置 v2 底座之后，v0.3.2 所需的**按需动态创建、空闲超时销毁、上限/最小值控制、整机内存总量限制与多租户公平 LRU 淘汰**只需在现有模块上做极小、无侵入的正交扩展即可自然生长出来。

## 6. 与 `workerd` 的架构差异对标

| 维度 | Cloudflare `workerd` | Capsid（v0.3.2 目标形态） |
| --- | --- | --- |
| **隔离单元** | 单进程内多线程 + 多 V8 Isolate 共享底层平台 | **独立 OS 沙箱子进程**（Namespaces + Landlock + Seccomp + cgroup）+ `quickjs-turbo` 零 JIT 引擎 |
| **冷启动与常驻成本** | Isolate 重建约数毫秒，但共享进程内 V8 基础开销高（实测空载/负载服务树数百 MiB） | 预编译字节码冷启动 **8.7 ms（10 KiB）/ 20.2 ms（1 MiB）**，单空闲 Worker 常驻 PSS 仅 **~7.2–8.2 MiB**（2 GiB 可驻留 700+ 实例） |
| **淘汰粒度与优势** | 驱逐引擎内冷 Isolate 上下文，依赖 V8 堆隔离 | 直接回收**空闲 Worker 进程**，OS 100% 彻底归还物理内存页，零跨租户堆碎片与侧信道残留 |
| **核心工程约束** | 进程内内存账本直接读取 V8 Heap 统计 | 需要在 Host 协调层做好 **冷启动排队缓冲**、**并发 fork 节流**、**异步进程回收不阻塞冷启动** 以及 **多租户内存公平防饿死** |

---

## 7. 多租户公平性与二阶段内存 LRU 淘汰机制

### 7.1 为什么多租户下不能只用“纯全局 LRU”？

在多租户混部场景下，若只维护一条全局空闲 LRU 链表：
- 租户 A 突发流量拉起 100 个 Worker 后进入空闲，排在 LRU 队头（最近刚用过）；
- 租户 B 每 30 秒才有 1 次请求，只维持着 1 个常驻热实例，排在 LRU 队尾；
- 一旦整机内存触及上限，**纯全局 LRU 会立刻杀掉租户 B 唯一的常驻实例来给租户 A 的 100 个空闲副本腾地方**——导致小租户每次请求都被迫承受冷启动（“吵闹邻居”效应）。

### 7.2 解决方案：二阶段公平内存 LRU（`GlobalEvictionCoordinator`）

只有同时满足以下 3 个条件的 Worker 才会进入可淘汰空闲池：
1. **纯空闲态**：`inflight_requests == 0` 且 `active_channels == 0`（绝不强杀任何在途 HTTP 请求、SSE 流或 WebSocket 连接）；
2. **高于保底线**：所属应用池当前存活实例数 `live > pool.minReady`；
3. **度过保护期**：距该 Worker 启动完成已超过最小驻留时间 `minResidency`（默认 `5s`，防止刚拉起就被淘汰的乒乓抖动）。

当整机工作集内存达到高水位 `highWatermark`（默认 `85%`）或新扩容请求受内存预算阻塞时，协调器按两阶段淘汰空闲实例，直至内存回落到低水位 `lowWatermark`（默认 `75%`，自带 10% 防抖迟滞带）：

- **Phase 1：租户超额纠偏（超公平份额者优先淘汰）**
  计算当前有驻留实例的活跃租户动态公平份额：
  $$\text{FairShare} = \frac{\text{MemoryBudget}}{N_{\text{active\_tenants}}}$$
  （若平台为 VIP 租户配置了保底软配额 `minSoftMemory`，则取 $\max(\text{FairShare}, \text{minSoftMemory})$）。
  筛选出当前实际内存占用 **超出其软份额** 的租户，优先从超额比例最大的租户内部，按 `lastActiveAt` 最早（LRU）淘汰空闲 Worker。这样大租户的冗余空闲副本会最先被回收，保护小租户的唯一热实例。
- **Phase 2：全局空闲 LRU（全员合规时按访问热度换出）**
  仅当所有租户均未超出各自软份额、但整机总内存依然高于水位（例如成百上千个小租户各占 1 个空闲实例撑满内存）时，才退化为全局纯 LRU，淘汰全机最久未访问的空闲实例。

---

## 8. 合理性审视：6 个关键工程隐患与修正闭环

将上述机制与 Capsid 现有 Host 源码（`generation_pool.h`、`worker_capacity_ledger.h`、`worker_supervisor.h`）逐行对账后，必须落实以下 **6 项工程修正**：

1. **严禁使用 Linux 内核 `cgroup memory.high` 做租户软水位（防 P99 延迟雪崩）**
   - **隐患**：Linux 内核在 `memory.high` 超线时会直接同步挂起（direct reclaim throttle）该 cgroup 下正在处理请求的活跃单线程事件循环，导致正常在途请求尾延迟飙升。
   - **修正**：内核层只读 `memory.current`（O(1) 零开销统计）+ 只写 `memory.max = hardMemory`（最后一道防宿主机 OOM 硬墙）；软水位淘汰 **100% 由用户态协调器仅针对纯空闲 Worker 异步执行**。
2. **内存计量双后端自动降级（兼容无 cgroup 委托容器与跨平台开发）**
   - **隐患**：非特权 Docker、rootless 容器或本地开发环境往往没有创建子 cgroup 并开启 `+memory` 委托的权限。
   - **修正**：有 cgroup v2 委托时直接 O(1) 读 `memory.current`；无委托时自动降级为**仅在 Worker 退回空闲态时及后台低频（5s）巡检时采样一次进程级 PSS/RSS（`/proc/<pid>/smaps_rollup` 等）并缓存**，热路径零系统调用开销。
3. **扩容踢旧采用异步 `reaping` 槽（绝不拖慢 9ms 冷启动）**
   - **隐患**：Worker 关闭带有优雅退出回收流程（`waitpid`）。若请求触发冷启动踢旧时同步阻塞等待旧进程彻底退出再 spawn 新进程，会把 9ms 字节码冷启动拖慢数百毫秒。
   - **修正**：一旦空闲受害者原子移出 READY 集合、确认零在途请求并下发不可逆 `shutdown`，其稳态配额**立即移交给新 Worker 并行 spawn**；旧进程转入有界 `reaping` 缓冲槽在后台几毫秒内异步回收。
4. **主动健康检查（`WorkerSupervisor`）防“假续命”隔离**
   - **隐患**：Host 默认每 `30s` 发送主动健康探针（`submit_probe`）。若探针刷新 `lastActiveAt`，会导致 `idleTimeout = 60s` 的空闲实例永远无法缩容到 `0`，且在 LRU 中永远伪装成刚活跃过。
   - **修正**：健康探针严格不刷新业务 `lastActiveAt`；当弹性池处于合法零实例休眠态（`live == 0 && minReady == 0`）时，`WorkerSupervisor` 静默跳过探测，不计失败也不唤醒实例。
5. **容量账本（`WorkerCapacityLedger`）拆分静态承诺与动态弹性池**
   - **隐患**：旧账本按固定池大小预留，若按 `maxWorkers` 扣减将无法实现高密超卖，且与蓝绿发布冲突。
   - **修正**：账本拆分为 **保底承诺槽（`committed = Σ minReady`，部署时硬校验 `<= workersTotal` 永不超卖）** + **全局弹性槽（`elastic = workersTotal - committed`，运行时按需抢占与 LRU 回收）**。蓝绿热更新只需预热 `max(1, new.minReady)` 个实例（`minReady == 0` 探活完成后可按空闲策略自然缩回 `0`），旧版本切入 `draining` 自然排空。
6. **WebSocket / SSE 长连接自然闭环**
   - 持有活跃长连接的 Worker 不进入空闲淘汰池；僵尸无流量连接由入口 `streamIdleTimeout` 与 `channelIdleTimeout` 超时清理后自动回归可淘汰态。存活期间其内存计入**所属租户自身的 `hardMemory` 配额**，不会挤占其他租户保底空间。

---

## 9. 易用性审视：在配置 v2 底座上的极简增量扩展

正因为第一部分已经打通了 `defaults` 继承链并清除了死字段，Phase 2 引入弹性池与多租户时**几乎不增加任何配置复杂度**：

### 9.1 租户身份归属 Host 控制面（严禁污染 `capsid.json`）

- `capsid.json` **不包含任何 `tenant` 字段**——既防止不可信应用包伪造租户身份，也让应用开发者完全无感；
- 仅在平台调用 Admin API `POST /v1/deploy` 时传入可选归属元数据 `{"app": "orders", "version": "v2", "tenant": "acme"}`；省略时默认 `tenant = app`（单团队部署零配置即可按应用粒度获得动态公平份额 LRU！）。

### 9.2 Phase 2 在配置 v2 上的增量字段预览

1. **`capsid.json`（及 `host.json#defaults.pool` / `maximums.pool`）**：
   保留 `pool.workers: N` 作为固定池简写（等价于 `minReady = maxWorkers = N`），弹性应用只需写：
   ```json
   "pool": {
     "minReady": 0,
     "maxWorkers": 8,
     "idleTimeout": "60s"
   }
   ```
   > 依靠已打通的 `defaults.pool`（默认提供 `idleTimeout: "60s"`, `queueRequests: 64`, `queueTimeout: "5s"`），应用甚至只需写 `"minReady": 0, "maxWorkers": 8` 两个数字，Scale-to-Zero 后的并发冷启动请求就会自动进入等待队列平滑等待 ~9ms，**绝不会因为忘配队列而直接抛 `503`**。

2. **`host.json#capacity`（一行开启整机内存预算 LRU）**：
   新增 `memory` 字段（支持 `"2GiB"` 极简字符串，自动推导 `85% / 75%` 水位与 `5s` 最小驻留期）：
   ```json
   "capacity": {
     "workersTotal": 512,
     "memory": "2GiB"
   }
   ```
   多租户平台按需增加可选顶层块 `tenancy`（单团队无需配置）：
   ```json
   "tenancy": {
     "defaultQuota": {
       "maxWorkers": 16,
       "hardMemory": "512MiB"
     },
     "overrides": {
       "vip-acme": {
         "maxWorkers": 64,
         "minSoftMemory": "256MiB",
         "hardMemory": "2GiB"
       }
     }
   }
   ```

### 9.3 运维排障透明化（拒绝“黑盒淘汰”与“黑盒 503”）

Admin 状态接口、结构化日志与 `/metrics` 指标必须对每次淘汰与拒绝显式分类打标：
- **`capsid_pool_evictions_total{app, tenant, reason}`**：`idle_timeout` | `tenant_overshare` | `global_lru` | `tenant_quota_swap`；
- **`capsid_pool_rejections_total{app, tenant, reason}`**：`queue_disabled` | `queue_full` | `queue_timeout` | `app_max_workers` | `tenant_max_workers` | `tenant_hard_memory` | `host_capacity_exhausted`。

---

## 10. 兼容性保障与分阶段落地路线图

1. **v1 永久冻结兼容**：`capsid/host-v1` 与 `capsid/app-v1` 保持现有解析与行为不变，存量配置零改动。
2. **分步推进（严格解耦，每步独立可测可交付）**：
   - **里程碑 A（配置 v2 基线重构 + 本地同构 CLI）**：落地第一部分的精简正交 `host-v2` / `app-v2`、打通 `defaults`/`maximums` 全部 8 个字段、合并本地 `single-worker`/`static-pool` 为 `managed` 产物同构模式，并加上“Schema 字段 100% 消费”门禁测试。
   - **里程碑 B（单池弹性伸缩与空闲销毁）**：打通 `0 <= minReady <= maxWorkers`、`idleTimeout`、冷启动等待队列、异步 `reaping` 缓冲以及主动健康探针隔离。
   - **里程碑 C（全局内存预算与多租户二阶段公平 LRU）**：落地 `capacity.memory`、双后端内存计量（cgroup v2 + PSS 采样降级）、Admin 部署 `tenant` 绑定、`tenancy` 配额切片与可观测性指标。
