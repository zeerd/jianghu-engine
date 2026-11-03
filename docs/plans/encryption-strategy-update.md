## Plan: 更新加密策略 - 可配置通讯密钥 + 密码加盐哈希

### TL;DR
当前存在两个安全问题：
1. **通讯加密密钥硬编码**：`StringUtil.cpp` 的 `decode_str` 使用固定 XOR 密钥 `"\15L\26A\18N\12W\25O\33O\6D"`，CLI 端 `protocol.h` 同步硬编码。
2. **密码明文存储**：`Sql.cpp::verify_user` 与 `SqlAccount.cpp::db_change_password` 直接在数据库 `cc_passwd` 列存明文（仅做单引号剥离）。

需改为：
- 通讯密钥从 `config.ini [security] comm_key` 读取，缺省回退到现有硬编码值（兼容旧版本/客户端）。
- 密码改用 **PBKDF2-HMAC-SHA256** 加盐存储：数据库新增 `cc_passwd_salt`、`cc_passwd_hash` 列；登录时验证 `PBKDF2(password, salt, 100000) == hash`；注册/改密时生成新盐并计算哈希。旧明文密码可平滑迁移（首次登录成功后自动转存为哈希）。

---

### 现状分析（关键文件与调用链）

| 组件 | 文件 | 关键函数/变量 | 说明 |
|------|------|---------------|------|
| 通讯解码 | `src/util/StringUtil.cpp` | `decode_str(char* msg)` | 硬编码 `const char* decode = "\15L\26A\18N\12W\25O\33O\6D"` |
| 通讯编码(CLI) | `cli/src/protocol.h` | `encode_str/decode_str` | 同密钥硬编码，需同步改为可配置 |
| 登录流程 | `src/net/Login.cpp` | `on_login` → `decode_str(arg)` → `verify_user(id, passwd)` | 登录帧先 XOR 解码，再明文比对密码 |
| 密码校验 | `src/db/Sql.cpp` | `verify_user(userid, passwd)` | SQL: `select * from jianghudb where cc_id=%s and cc_passwd='%s'` |
| 改密/注册 | `src/db/SqlAccount.cpp` | `db_change_password`, `db_register_account` | 直接写入 `cc_passwd` 明文（经 `normalize_password` 剥单引号） |
| 配置加载 | `src/util/IniParser.h` | `ini_load`, `ini_get` | 已有 INI 解析基础设施，`g_ini_data` 全局映射 |
| 随机数 | `src/util/Random.cpp` | `CUtils::random/urandom` | 可用于生成盐值（需扩展：生成加密安全随机字节） |

---

### 实施步骤

#### Phase 1: 通讯密钥可配置化（独立、可先行交付）

1. **扩展配置结构**
   - `config.ini` 新增 `[security]` 节：
     ```ini
     [security]
     ; 通讯 XOR 密钥（十六进制或原始字节转义，如 \15L\26A...），留空则用内置默认值
     comm_key =
     ```
   - `src/util/IniParser.h` 已支持任意 section/key，无需改动。

2. **重构 `decode_str` 为可配置版本**
   - `src/util/StringUtil.h/cpp`：
     - 新增 `set_comm_key(const char* key)` / `get_comm_key()`（内部静态缓存，启动时从 `ini_get("security", "comm_key", DEFAULT_KEY)` 初始化）。
     - `decode_str` 改用缓存的密钥而非硬编码常量。
     - 保持函数签名不变（`char* decode_str(char* msg)`），调用方零改动。

3. **同步 CLI 端**
   - `cli/src/protocol.h`：`encode_str` 读取同一配置项（CLI 也加载 `config.ini` 或共享同一份），或约定 CLI 从服务端下发的握手包获取密钥（V2 方案，暂不做，先按配置文件同步）。

4. **单元测试**
   - `tests/unit/test_encoding.cpp`：验证不同密钥下 encode/decode 互逆、空密钥回退默认值、配置变更即时生效。

#### Phase 2: 密码加盐哈希（核心安全增强）

1. **数据库 Schema 变更**
   - `jianghudb` 表新增两列：
     - `cc_passwd_salt` VARCHAR(32) NOT NULL DEFAULT '' -- Base64 编码的 16 字节盐
     - `cc_passwd_hash` VARCHAR(64) NOT NULL DEFAULT '' -- Base64 编码的 32 字节 PBKDF2-HMAC-SHA256 哈希
   - 保留 `cc_passwd` 列（兼容旧数据），迁移后可标记废弃。

2. **引入加密工具模块**
   - 新建 `src/util/Crypto.h/cpp`：
     - `generate_salt(uint8_t out[16])` — 调用 `CryptGenRandom` (Windows) 或 `/dev/urandom`。
     - `pbkdf2_hmac_sha256(const char* pass, const uint8_t* salt, size_t salt_len, uint32_t iterations, uint8_t out[32])` — 使用 Windows CNG (BCrypt) 或 OpenSSL（若已链接）。
     - `base64_encode/decode` 工具函数。
     - 常量：`PBKDF2_ITERATIONS = 100000`。

3. **改造 `verify_user`（登录校验）**
   - `src/db/Sql.cpp::verify_user`：
     - 查询改为 `select cc_passwd, cc_passwd_salt, cc_passwd_hash from jianghudb where cc_id=%s`。
     - 逻辑：
       1. 若 `cc_passwd_hash` 非空 → 用 PBKDF2 验证 `hash == PBKDF2(pass, salt, 100000)`。
       2. 否则（旧明文数据）→ 明文比对 `cc_passwd == pass`，**验证通过后**自动生成盐、算哈希、执行 `UPDATE ... SET cc_passwd_salt=?, cc_passwd_hash=?, cc_passwd='' WHERE cc_id=?` 完成无感迁移。
     - 返回 `bool` 保持原签名。

4. **改造 `db_change_password` / `db_register_account`（写入路径）**
   - `src/db/SqlAccount.cpp`：
     - 生成新盐 `generate_salt(salt)`。
     - 计算 `hash = PBKDF2(new_pass, salt, 100000)`。
     - 写入 `cc_passwd_salt`、`cc_passwd_hash`，`cc_passwd` 置空。
     - `normalize_password` 保留（剥单引号），在哈希前调用。

5. **次级密码 `cc_verifypass` 同步处理**
   - 同理新增 `cc_verifypass_salt`、`cc_verifypass_hash`，`db_change_verify_password` / `db_verify_password` 同步改造。

6. **单元测试**
   - `tests/unit/test_db_layer.cpp`：
     - 验证新注册账号密码以哈希存储、登录验证通过。
     - 验证旧明文账号首次登录自动迁移为哈希。
     - 验证改密后哈希更新、旧哈希失效。
     - 验证次级密码同逻辑。
   - `tests/unit/test_crypto.cpp`（新建）：PBKDF2 向量测试、盐唯一性、Base64 往返。

#### Phase 3: 集成验证与回归

1. **集成测试**
   - 运行 `.\run-integration-tests.ps1`：登录、注册、改密、次级密码全链路绿灯。
   - 关注 `lane_*.sce` 中涉及登录/改密的片段（`register.sce`、`chpass.sce` 等）。

2. **覆盖率门槛**
   - `.\run-coverage.ps1`：新增代码行覆盖 ≥ 90%，`delta ≥ 0`。

3. **配置文件示例更新**
   - `data/config.ini` 补充 `[security]` 节注释与示例值。

---

### 关键文件修改清单

| 文件 | 修改类型 | 说明 |
|------|----------|------|
| `data/config.ini` | 新增配置项 | `[security] comm_key =` |
| `src/util/StringUtil.h/cpp` | 重构 | `decode_str` 使用可配置密钥，新增 `set_comm_key/get_comm_key` |
| `cli/src/protocol.h` | 同步修改 | `encode_str` 读取同一配置 |
| `src/util/Crypto.h/cpp` | **新建** | PBKDF2、盐生成、Base64 |
| `src/db/Sql.cpp` | 修改 `verify_user` | 支持哈希验证 + 明文自动迁移 |
| `src/db/SqlAccount.cpp` | 修改 `db_change_password`/`db_register_account`/`db_change_verify_password` | 写入哈希+盐，清空明文列 |
| `src/db/Sql.h` | 同步声明 | 若签名变更（不变） |
| `tests/unit/test_encoding.cpp` | 扩展 | 可配置密钥测试 |
| `tests/unit/test_crypto.cpp` | **新建** | 密码学原语测试 |
| `tests/unit/test_db_layer.cpp` | 扩展 | 哈希存储/验证/迁移测试 |
| `jianghu_core.vcxproj` / `tests/unit/tests.vcxproj` | 新增编译单元 | `Crypto.cpp` 加入静态库与测试工程 |

---

### 验收标准

1. **通讯密钥可配置**
   - 修改 `config.ini` 的 `comm_key` 后，服务端与 CLI 能正常通讯（登录、指令收发）。
   - 留空 `comm_key` 时回退到内置默认值，旧客户端不升级也能连接。

2. **密码加盐哈希**
   - 新注册账号：数据库 `cc_passwd` 为空，`cc_passwd_salt`/`cc_passwd_hash` 非空。
   - 登录验证：PBKDF2 校验通过。
   - 旧账号首次登录：自动迁移为哈希存储，后续登录走哈希校验。
   - 改密/次级密码：同逻辑生效。
   - 单测覆盖上述全分支。

3. **无回归**
   - `run-coverage.ps1` 全绿，覆盖率不降。
   - `run-integration-tests.ps1` 全绿（含服务端日志 ERROR 白名单检查）。

---

### 依赖与风险

- **Windows CNG (BCrypt)**：需链接 `Bcrypt.lib`，已在 Windows SDK 中，无额外依赖。
  - `BCryptOpenAlgorithmProvider` / `BCryptGenerateSymmetricKey` / `BCryptDeriveKeyPBKDF2` / `BCryptGenRandom`。
- **数据库迁移**：`ALTER TABLE` 需在服务器启动时自动执行（`init_sql_server` / `restore_data_table` 阶段），或提供一次性迁移脚本。建议在 `Sql.cpp::restore_data_table` 中 `IF NOT EXISTS` 追加列。
- **CLI 同步**：CLI 端也需重新编译以拾取新 `protocol.h`；`run-integration-tests.ps1` 会自动构建 CLI。
- **性能**：PBKDF2 100k 迭代约 50-100ms/次（单核），登录路径可接受；如需可调低至 50k 或异步化（后续优化）。

---

### 进一步考虑（可选增强）

1. **通讯加密升级到 AES-GCM**
   - 当前 XOR 仅混淆，非加密。长期可引入会话密钥协商（ECDH）+ AES-GCM，但涉及协议重构，超出本任务范围。

2. **Argon2id 替代 PBKDF2**
   - 更抗 GPU 破解，但需引入第三方库（如 `libargon2`），增加部署复杂度。PBKDF2-HMAC-SHA256 100k 迭代在当前威胁模型下足够。

3. **密钥轮换机制**
   - `comm_key` 支持多版本（`comm_key_v1`, `comm_key_v2`...），服务端按版本号解码，平滑轮换。暂不做。

---

### 执行顺序建议

1. **Phase 1 先行**（通讯密钥可配置）：改动面小、风险低、可独立验收。
2. **Phase 2 核心**（密码哈希）：需数据库 Schema 变更，建议在测试环境先跑迁移脚本验证。
3. **Phase 3 集成**：全量回归。

每阶段完成后跑 `run-coverage.ps1` 与 `run-integration-tests.ps1` 确认无回归。