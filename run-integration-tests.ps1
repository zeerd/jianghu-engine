# run-integration-tests.ps1 — 集成测试入口（lua-sample 多账号并行 E2E）
#
# 与单元测试入口 run-coverage.ps1 并列。作用：
#   1. 构建 Release|Win32 的服务器 bin\jianghu.exe 与 CLI bin\cli\jianghu_cli.exe；
#   2. 用 tests\integration\config.ini 启动真实服务器（服务端命令行 --config 指定配置，
#      不再临时改写 data\config.ini），独立测试库 + 独立端口；
#   3. **一次性注册 $Workers 个账号**（并行），再**同时开跑各道剧本**
#      （scenarios\lane_*.sce，每道一串 include 的功能片段，彼此互不依赖；
#      例外是互动道：L5/L6 配对，注册阶段互相注入对方角色名与账号
#      （vars 的 peer/peer_acct 键）会合后做玩家对玩家的互动；L7 观战道
#      单向依附 L5（本道 vars 收 peer 键、L5 收 spectator 键），为 L5/L6
#      的 PK 段提供房外观战/加入的第三方）；
#      各道分别注册、分别采购、分别推进，只在共享世界里「钉给唯一一道」的
#      资源上互斥，其余全靠服务器并发推进——墙钟时间约为最长的一道而非各道之和；
#   4. 在 OpenCppCoverage 下采集 src 行覆盖，导出 Cobertura / HTML / binary；
#      所有道打的是同一个服务器进程，覆盖率天然合并，无需额外聚合；
#      统计时按叶子文件名跳过 LuaSyntaxChecker.cpp 等结构性不可达文件
#      （口径对齐 run-coverage.ps1 的 $excludedFiles；CmdWizard* 已因 L9
#      巫师道落地而移出排除表——保持清单可见，但 2026-11-01 决策**不追求
#      巫师命令覆盖度**：以 §6 决策认领，不设逐条消化目标）。
#   5. 在控制台输出各道结果、行覆盖率与「未命中函数」清单；清单过长时只报数字，
#      完整清单写入 coverage_history\e2e-sample-unhit-functions.txt；
#   6. 任一道失败即整体失败：只输出各道结果与失败原因，不展示覆盖率与未命中
#      函数（避免覆盖率刷屏淹没失败信息；报告文件仍会导出，便于排查）。
#   7. 崩溃检测：启动前先把**上一轮遗留**的旧 dump 归档到仓库根 crash_dumps\
#      （避免在仓库根无限堆积），再快照仓库根 *.dmp，收尾停服后 diff——服务器
#      未处理异常（关服路径崩溃）会生成新 dump，判定为整体失败。**本轮新产生的
#      dump 原地留在仓库根**并打印完整路径，绝不挪走：挪到 %TEMP% 会让人看不到
#      崩溃证据、误以为崩溃已修好（「剧本全绿、服务器其实崩了」同理要防）。
#   8. 耗时监控：每次 PASS 运行把「剧本执行总耗时」（注册 + 各道并行，不含构建与
#      覆盖率导出）连同各道耗时/步数/场景清单追加到
#      coverage_history\e2e-timing-history.jsonl（保留最近 50 条），并与前次同类
#      通过运行（同模式、同 workers、同覆盖率开关）对比；增量超过 -DurationRegressionSec
#      （默认 10 秒）时 diff 各道 include 场景清单归因是否新增场景导致，并按
#      关键路径估算「移入现有道 / 新增一条道」的拆分收益。
#   9. 简单模式（-Simple）：单条冒烟道验证「起服 → 登录 → look → 行走 → 关服」的
#      最小命令面，预算 30s（口径=整段墙钟，不含构建）。账号自举免注册、强制重建
#      测试库、强制无覆盖率（汇总另写 summary-e2e-smoke.txt，不覆盖完整模式的
#      覆盖率汇总）；失败判定四通道与完整模式同一套。
#      ⚠ 定位：仅供人类做「服务器还活着吗」的快速确认。AI/Agent 禁用——AI 的
#      验证与交付验收必须跑完整模式，不许拿 -Simple 的绿当验收结论。
#  10. persist 试验台（**并发中间重启机制**，仅完整模式）：一条后台任务在独立
#      端口（config.persist.ini，6612）+ 独立库（e2e_persist.db）上驱动第二台
#      服务器，按「跑段 → 停服 → 重启 → 跑下段」的 **N 段循环**执行段表
#      （$PersistSegments；本次两段：persist_pre → 重启 → persist_post，断言
#      can_save 挑战计数跨重启继续累加）。任务在阶段 1 之前启动，**与注册和
#      全部道并行**——重启发生在各道跑到一半时，不占关键路径；段表加一项即
#      可扩成三段（pre→重启→mid→重启→post），供后续需要重启/干净态的测试复用。
#      覆盖率多路合并：主服与各试验台实例各导二进制，全部结束后跑一趟
#      合并通行证（--input_coverage 可多次传入，启动时校验文件存在）出最终报告。
#  11. reload 试验台（**热重载 E2E**，仅完整模式）：一条后台任务在独立端口
#      （config.reload.ini，6613）+ 独立库（e2e_reload.db）+ 私有脚本副本
#      （test_tmp\e2e_reload\scripts = lua-sample 每轮整树复制 + fixture A 变体）
#      上起第二台服务器，跑一条连续剧本链（scenarios\reload\reload_suite.sce）：
#      改文件（filecopy/filedelete 写在片段里）→ 等重载结果（sys 广播屏障）→
#      断言行为切换——覆盖自动重载主路径、失败兜底、手动 reload 回执、共享
#      模块 require 缓存语义与 mtime-noop。任务与各道并行、不占关键路径；
#      收尾由任务断言服务端日志（auto-reload 行在案 / noop 探针未被误重载 /
#      失败尝试在案），结果 JSON 并入失败判定四通道；覆盖率第四路
#      coverage_history\e2e_reload.cov 随合并通行证并入总报告。编排与调试
#      入口见 tests\integration\reload_testbed.ps1（可前台直跑快速迭代剧本）。
#
# 依赖：MSBuild（VS）、OpenCppCoverage。数据库为 SQLite（tests\integration\e2e.db），
#        由服务端启动时自动建库建表，无需任何外部数据库服务。
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1 -SkipBuild -NoCoverage
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1 -Workers 2 -NoCoverage
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1 -CliExe C:\tmp\jhcli.exe
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1 -Simple
#     简单模式（冒烟）：只跑登录/look/行走一条道，整段墙钟预算 30s（不含构建）。
#     ⚠ 仅供人类快速确认；AI/Agent 禁用——AI 的验证与交付验收必须跑完整模式。
#
# 说明：
#   - 使用独立 SQLite 文件库（tests\integration\config.ini 的 e2e.db），不污染业务库；
#     每个账号用唯一角色名，因此无需清库。需要全新库时用 -ResetDb 删除库文件，
#     服务端下次启动自动重建。
#   - 覆盖率与「未命中函数」都是近似口径，详见 docs\design\integration-test.md §5.3。
#   - 各道的变量/报告/日志在仓库根 test_tmp\e2e_lanes\（lane<N>.vars/.report/.log），
#     与单测/CLI 测试共用 test_tmp\（.gitignore 已排除）：系统 %TEMP% 无人清理，
#     测试残留会无限堆积。
#   - 进程管理只认「监听测试端口的 jianghu」：启动前只回收上次残留的测试服务器，
#     收尾只停本脚本起的那台，不会按进程名误杀你在其他端口（如 6600）跑的服务器；
#     但也因此不要把功能测试服务器开在测试端口（tests\integration\config.ini 的 port）上。

param(
    # CLI 可执行文件；默认 bin\cli\jianghu_cli.exe
    [string]$CliExe = "",
    # 跳过 MSBuild 构建（直接使用现有 bin\jianghu.exe / CLI）
    [switch]$SkipBuild,
    # 跳过 OpenCppCoverage（快速调试剧本，不出覆盖率报告）
    [switch]$NoCoverage,
    # 运行前重建测试库：删除 SQLite 库文件（服务端启动自动重建）
    [switch]$ResetDb,
    # 给各道 CLI 加 --dbg：把服务端报文（RTNINFO/STATE 等）打进 lane 日志，
    # 用于定位「客户端属性值和服务端对不上」这类问题
    [switch]$Dbg,
    # ⚠ 仅供人类做快速确认（「服务器还活着吗」）：单条冒烟道，整段墙钟预算 30s
    # （不含构建）。AI/Agent 禁用——AI 的验证与交付验收必须跑完整模式，不许拿
    # -Simple 的绿当验收结论。
    # 机制：只跑登录 / look / 行走一条道，巫师自举账号免注册，强制 -NoCoverage、
    # 强制重建测试库、忽略 -Workers；与完整模式共用失败判定四通道与耗时历史，
    # 历史记录带 mode 字段，两种模式互不作为 Δ 对比基线。
    [switch]$Simple,
    # 单个 wait/assert 默认超时（毫秒）
    [int]$StepTimeout = 20000,
    # 等待服务器端口就绪的超时（秒）。首次在 OpenCppCoverage 下加载 PDB 可能较慢
    [int]$ServerReadyTimeoutSec = 180,
    # 长时间无输出时的心跳间隔（秒）
    [int]$HeartbeatSec = 10,
    # 控制台最多逐条打印多少个未命中函数；超出则只报数字
    [int]$UnhitListLimit = 60,
    # 并行的账号（道）数。剧本按功能域拆成 10 条道（L1~L4 互不干扰 + L5/L6
    # 配对互动道 + L7 观战道单向依附 L5 + L8 山贼试炼单向依附 L3 的错峰信号
    # + L9 巫师服务道（账号自举、不注册）+ L10 练功道（N4 自 L3 拆出）；
    # 配对与依附关系都必须同时启用），每道一个账号，同时注册、同时开跑；
    # 调小会缩短并行度并丢掉未启用道的覆盖。
    [int]$Workers = 10,
    # 耗时监控：本次剧本执行总耗时较前次同类通过运行增加超过该秒数时，
    # 归因是否新增场景导致，并探索拆分（移道 / 新增道）的可能性。
    # （Δ 归因是纯诊断；通过与否看下面的绝对预算）
    [int]$DurationRegressionSec = 10,
    # 剧本执行总耗时（注册 + 各道并行墙钟）的**绝对预算**。超预算即高声告警
    # （不判 FAIL——预算是护栏不是门禁）。相对基线的 Δ≤N 门禁已废除：滑动基线
    # 每次放行 N 秒、单调爬升无上限；绝对预算把「合法」钉死为代码里的常数，
    # 调整必须改本默认值走 diff 评审，留下显式记录。
    [int]$DurationBudgetSec = 420
)

$ErrorActionPreference = "Stop"

# 简单模式的派生默认值（任何阶段执行前定死）：
#   · 强制无覆盖率——覆盖率与未命中函数是完整模式的口径，冒烟不采集；
#   · 强制重建测试库——存档的 start_city 是上次离线房间，不清库时巫师账号首登
#     落点不确定，login 片段的起始房间断言会漂；重建库后首登必落 start_room；
#   · 预算默认 30s（仅当未显式传 -DurationBudgetSec 时切换，显式传参以传参为准）。
if ($Simple) {
    $NoCoverage = $true
    $ResetDb = $true
    if (-not $PSBoundParameters.ContainsKey('DurationBudgetSec')) { $DurationBudgetSec = 30 }
}

# 起点计时（耗时监控用，文首第 8 条）；各阶段秒数在对应阶段赋值，未到达即为 $null
$tScriptStart = Get-Date
$tBuildSec = $null
$tServerReadySec = $null
$tRegSec = $null
$tLaneSec = $null
$tExportSec = $null
# persist 试验台（与各道并行，不进 exec 关键路径）：结果从任务的 JSON 回收，
# pre/post 段耗时与段间重启耗时只作诊断展示
$sideJob = $null
$sideResult = $null
# reload 试验台（文首第 11 条）：同为并行后台任务，链耗时只作诊断展示
$reloadJob = $null
$reloadResult = $null
# 注意：此处早于 $RepoRoot 赋值，直接用 $PSScriptRoot（两者同值）
$SideResultFile = Join-Path $PSScriptRoot "test_tmp\e2e_lanes\persist_testbed.result.json"
# reload 试验台结果（文首第 11 条）：任务自产 JSON，主流程回收进失败判定
$ReloadResultFile = Join-Path $PSScriptRoot "test_tmp\e2e_lanes\reload_testbed.result.json"
# 整段墙钟（简单模式预算口径，文首第 9 条）：构建之后起算，至关服与观测收尾，
# 不含覆盖率导出；未到起算点即为 $null（构建失败等，此时也不会有耗时记录）
$tWallStart = $null
$tWallSec = $null
# 注册阶段是否真的跑过：账号自举模式没有注册阶段，耗时记录的 reg_scenarios 为空
$regRan = $false

# --- 路径 -------------------------------------------------------------------
$RepoRoot = $PSScriptRoot
$IntDir = Join-Path $RepoRoot "tests\integration"
$ConfPath = Join-Path $IntDir "config.ini"
$ScenarioDir = Join-Path $IntDir "scenarios"
$CliConf = Join-Path $IntDir "cli.conf"
# persist 试验台（文首第 10 条）：独立端口/独立库的第二服务器配置
$SideConfPath = Join-Path $IntDir "config.persist.ini"
$SideCliConf = Join-Path $IntDir "cli.persist.conf"
# reload 试验台（文首第 11 条）：独立端口/独立库/私有脚本副本的第二服务器配置
$ReloadConfPath = Join-Path $IntDir "config.reload.ini"
$ReloadCliConf = Join-Path $IntDir "cli.reload.conf"
$ReloadTestbedScript = Join-Path $IntDir "reload_testbed.ps1"
$ReloadFixturesDir = Join-Path $IntDir "reload_fixtures"
$ReloadScriptsRoot = Join-Path $RepoRoot "test_tmp\e2e_reload\scripts"
$ReloadDbPath = Join-Path $RepoRoot "test_tmp\e2e_reload\e2e.db"
$ServerProj = Join-Path $RepoRoot "jianghu.vcxproj"
$CliProj = Join-Path $RepoRoot "cli\jianghu_cli.vcxproj"
# 集成测试用**专属**服务器 exe（bin\e2e\jianghu.exe）：与正常使用
# bin\jianghu.exe（如功能测试服跑在 6600）互不抢同一个文件锁，可并存。
$E2EDir = Join-Path $RepoRoot "bin\e2e"
$ServerExe = Join-Path $E2EDir "jianghu.exe"
$ReportDir = Join-Path $RepoRoot "coverage_report_e2e_sample"
$CovBin = Join-Path $RepoRoot "coverage_history\e2e_sample.cov"
# 三路覆盖率的中间产物：主服一路（试验台各段由后台任务自命名
# e2e_persist_seg<k>.cov，末尾合并通行证把全部输入汇成上面的 $CovBin）。
$CovBinMain = Join-Path $RepoRoot "coverage_history\e2e_sample_main.cov"
$SideCovGlob = Join-Path $RepoRoot "coverage_history\e2e_persist_seg*.cov"
# reload 试验台第四路（单服务器单寿命，一路即可）
$ReloadCovGlob = Join-Path $RepoRoot "coverage_history\e2e_reload*.cov"
# 汇总文件分模式：简单模式不采集覆盖率，若复用同名文件会把完整模式的覆盖率
# 汇总（提交前清单的 delta 对比基准）冲掉——冒烟写自己的 summary-e2e-smoke.txt。
$SummaryFile = Join-Path $RepoRoot ("coverage_history\" +
    $(if ($Simple) { "summary-e2e-smoke.txt" } else { "summary-e2e-sample.txt" }))
$UnhitFile = Join-Path $RepoRoot "coverage_history\e2e-sample-unhit-functions.txt"
# N3 分段：已认领（决策归档）清单——命中的归「§6 决策认领」段，其余归「待推进」段。
# 新增甄别归档时往该文件追加一行（格式=清单原行）；逐函数明细只进本清单（阶段工具），
# design §6 只承载域级裁定。
$ClaimedFile = Join-Path $RepoRoot "coverage_history\e2e-unhit-claimed.txt"
# 耗时历史：每次 PASS 追加一条 JSON（各阶段耗时 + 各道场景清单），供与前次对比归因
$TimingHistoryFile = Join-Path $RepoRoot "coverage_history\e2e-timing-history.jsonl"
$TimingKeepCount = 50
$OccExe = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"

# --- 道（lane）定义 -----------------------------------------------------------
# 每条道 = 一个独立账号 + 一串 include 串起来的功能片段，彼此互不依赖：
# 各道分别注册、分别采购、分别推进，靠共享世界里「钉给唯一一道」的资源隔离。
# 改这里即可增删道（Scripts 对应 scenarios\ 下的 lane*.sce）。
$Lanes = @(
    @{ Id = 1; Desc = "山脚基础+房屋"; Script = "lane_foot.sce" },
    @{ Id = 2; Desc = "武学链"; Script = "lane_skill.sce"; WizardId = 9 },
    @{ Id = 3; Desc = "山顶战斗尾"; Script = "lane_combat.sce"; WizardId = 9 },
    # L4 依赖巫师服务（WizardId=9）：NPC 师徒家门查询（场景 C 的 family/user
    # 回归断言）走 §2.6 的巫师直读 dbase 通道，本道自己没有只读 dbase 的命令。
    @{ Id = 4; Desc = "社交生活"; Script = "lane_social.sce"; WizardId = 9 },
    # L5/L6 是配对互动道（PeerId 互相指认）：注册阶段把对方角色名写进对方
    # vars（peer 键），剧本以 $peer 会合后做玩家互动（组队/私聊/赠送/交易等）。
    # L5 的 WitnessId=7 指向观战道：L5 vars 收 spectator 键（对方角色名），
    # 注册后同样互注 spectator_acct——pk/fight 段跨房 tell 开战信号用。
    # Gender 是注册性别（register.sce 的 menu $gender）：wakeup2 双人同睡要求
    # 异性，故配对道 L5=男性 / L6=女性；其余道不声明，注入时默认男性。
    @{ Id = 5; Desc = "互动A"; Script = "lane_interaction_a.sce"; PeerId = 6; WitnessId = 7; Gender = "男性"; WizardId = 9 },
    @{ Id = 6; Desc = "互动B"; Script = "lane_interaction_b.sce"; PeerId = 5; Gender = "女性" },
    # L7 观战道（PeerId=5 单向依附 L5）：L5/L6 打 PK 与死亡链时它待在
    # 会合房间房外，做比试房观战（guard）与战斗房加入（join）。
    @{ Id = 7; Desc = "观战"; Script = "lane_spectate.sce"; PeerId = 5 },
    # L8 山贼试炼（PeerId=3 单向依附 L3）：黑风寨是全局单例副本，本道先入
    # 打山贼（l_receive_wound 反噬 + l_perform_action 随机出招的唯一触发面），
    # 打完（或败退回山脚）tell L3「黑风寨已清」，L3 收到才进寨——两道零
    # 同时开战窗口（§2.4）。Attrs 是注册天赋四维（10~30/总和 84）：本道取
    # str10 int14 con30 dex30——con30 把 max_hp 拉到 1260（10 级），配合战前
    # 金创顶 eff，吃出约 34 秒的战斗窗口（反震 97% + 山贼出招 ≈99% 的触发面）。
    @{ Id = 8; Desc = "山贼试炼"; Script = "lane_raid.sce"; PeerId = 3; Attrs = "10 14 30 30"; WizardId = 9 },
    # L9 巫师服务道：账号随服务端自举（cc_id=1 巫师一号，ensure_wizard_accounts
    # 随启动建号），不走注册流程——NoRegister：vars 直写 acct=1、pass 取
    # config [wizard] password（缺省 12345678，与 Sql.cpp 同缺省）。需求道
    # （L8/L5/L3）声明 WizardId=9 声明依赖，启动校验同 PeerId/WitnessId。
    @{ Id = 9; Desc = "巫师服务"; Script = "lane_wizard.sce"; NoRegister = $true },
    # L10 练功道（N4 L3 拆分产物）：承接原 L3 的五段练功/学习任务——
    # upgrade 门槛链（levelup_heal）、force 链（force_for_exercise）、两轮收工
    # 打坐（exercise_max_mp）、halt 保 mp（halt_exercise）、收工不抬升补全
    # （exercise_no_raise，L9 供大上限）、throwing 链（learn_throwing）。
    # L2 已 288s 放不下 +130s，故开新道（§3.3「不足则评估新练功道」）；
    # 约 190s，非关键路径。WizardId=9：no_raise 段要巫师 call set max_mp。
    @{ Id = 10; Desc = "练功"; Script = "lane_train.sce"; WizardId = 9 }
)

if ($Workers -lt 1) { $Workers = 1 }

# 简单模式（文首第 9 条）：只有一条冒烟道——巫师自举账号（NoRegister，跳过注册
# 阶段）+ 登录/行走片段，不设 peer/witness/wizard 依赖，下方依赖校验自然全过。
if ($Simple) {
    $Lanes = @(
        @{ Id = 1; Desc = "简单冒烟"; Script = "lane_simple.sce"; NoRegister = $true }
    )
    if ($PSBoundParameters.ContainsKey('Workers') -and $Workers -ne 1) {
        Write-Warning "-Simple 忽略 -Workers=$Workers：简单模式固定 1 条冒烟道"
    }
    $Workers = 1
}

if ($Workers -gt $Lanes.Count) { $Workers = $Lanes.Count }
$ActiveLanes = @($Lanes | Select-Object -First $Workers)
if ($Workers -lt $Lanes.Count) {
    Write-Warning "Workers=$Workers 只跑 $($ActiveLanes.Count)/$($Lanes.Count) 条道，未启用的道不会执行，覆盖率会低于完整运行。"
}

# 配对互动道必须成对启用：L5/L6 互为 peer，只启用一半会因找不到对方角色名
# 而在会合片段超时，这里提前给出明确报错。同理，L5 依赖观战道 L7（spectator
# 注入 + PK 段信号）、L7 依赖 L5（peer 注入），L8 依赖 L3（错峰信号的 tell 目标），
# 任何一半缺位都在此拦下。
foreach ($lane in $ActiveLanes) {
    if ($lane.PeerId -and -not (@($ActiveLanes | Where-Object { $_.Id -eq $lane.PeerId }) | Select-Object -First 1)) {
        throw "道 L$($lane.Id)（$($lane.Script)）依赖配对道 L$($lane.PeerId)（peer 互注），但 L$($lane.PeerId) 未启用：请把 -Workers 调到 $(@($Lanes | Where-Object { $_.Id -eq $lane.PeerId }) | Select-Object -First 1 | ForEach-Object { $_.Id }) 及以上。"
    }
    if ($lane.WitnessId -and -not (@($ActiveLanes | Where-Object { $_.Id -eq $lane.WitnessId }) | Select-Object -First 1)) {
        throw "道 L$($lane.Id)（$($lane.Script)）依赖观战道 L$($lane.WitnessId)（spectator 注入与 PK 段信号），但 L$($lane.WitnessId) 未启用：请把 -Workers 调到 $($lane.WitnessId) 及以上。"
    }
    if ($lane.WizardId -and -not (@($ActiveLanes | Where-Object { $_.Id -eq $lane.WizardId }) | Select-Object -First 1)) {
        throw "道 L$($lane.Id)（$($lane.Script)）依赖巫师服务道 L$($lane.WizardId)（巫师账号改定属性/技能），但 L$($lane.WizardId) 未启用：请把 -Workers 调到 $($lane.WizardId) 及以上。"
    }
}

# persist 试验台的**段表**（N 段通用机制，头注第 10 条）：每项 = 试验台的
# 一个服务器生命周期，段与段之间即一次重启；后台任务按本表循环执行，全程与
# 各道并行。字段：Id 段名（变量/报告/日志文件名）｜Suffix 账号名后缀（避开
# 道序号数字，段间不得重名）｜Script 段剧本｜Inject 段前注入（$null=无；
# "expect_n_plus1"=读上一段 vars 的 n、注入 expect_n=n+1——跨重启断言用）。
# 扩成三段只需追加一项并按需新增一种 Inject，任务循环与覆盖率合并自动适配。
$PersistSegments = @()
if (-not $Simple) {
    $PersistSegments = @(
        @{ Id = "pre"; Suffix = "p"; Script = "persist\persist_pre.sce"; Inject = $null },
        @{ Id = "post"; Suffix = "q"; Script = "persist\persist_post.sce"; Inject = "expect_n_plus1" }
    )
}

# 每道独立的变量文件 / 剧本报告 / 输出日志（变量文件承载本道自己的 acct）。
# 落仓库根 test_tmp\ 而非系统 %TEMP%：临时文件必须留在项目内可查、可随仓库清理。
$WorkDir = Join-Path $RepoRoot "test_tmp\e2e_lanes"
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
foreach ($lane in $ActiveLanes) {
    $lane.VarsFile = Join-Path $WorkDir ("lane{0}.vars" -f $lane.Id)
    $lane.ReportFile = Join-Path $WorkDir ("lane{0}.report" -f $lane.Id)
    $lane.LogFile = Join-Path $WorkDir ("lane{0}.log" -f $lane.Id)
}

# ===========================================================================
# 源码扫描：近似提取「未命中函数」
# ---------------------------------------------------------------------------
# OpenCppCoverage 的 Cobertura 导出不含函数级数据（<class> 的 <methods/> 是空的），
# 这里按本仓库的 Allman 风格（函数体 '{' 独占一行）识别函数定义，再结合行命中数据，
# 把「函数体内没有任何一行被命中」的函数判为未命中。
#
# 已知局限：单行风格函数体（`void f() {`）、模板函数、lambda 不计入；预处理器
# 未编译分支也会被计入；编译器内联可能让实际执行过的函数显示为 0；只在 Cobertura
# 出现过的文件里扫描，因此不会把未插桩的文件误报为未命中。
# ===========================================================================

function ConvertTo-CodeLine {
    param(
        [string]$Line,
        [ref]$InBlockComment
    )
    $s = $Line
    if ($InBlockComment.Value) {
        $end = $s.IndexOf('*/')
        if ($end -lt 0) { return '' }
        $s = $s.Substring($end + 2)
        $InBlockComment.Value = $false
    }
    while ($true) {
        $st = $s.IndexOf('/*')
        if ($st -lt 0) { break }
        $en = $s.IndexOf('*/', $st + 2)
        if ($en -lt 0) {
            $InBlockComment.Value = $true
            $s = $s.Substring(0, $st)
            break
        }
        $s = $s.Substring(0, $st) + ' ' + $s.Substring($en + 2)
    }
    # 字符字面量先于字符串字面量，避免 '"' 被误当字符串起点
    $s = [regex]::Replace($s, "'(?:\\.|[^'\\])*'", "''")
    $s = [regex]::Replace($s, '"(?:\\.|[^"\\])*"', '""')
    $ci = $s.IndexOf('//')
    if ($ci -ge 0) { $s = $s.Substring(0, $ci) }
    return $s
}

# 控制语句 / 非函数块开头关键字
$script:NonFunctionHeads = 'if|for|while|switch|catch|else|do|try|return|namespace|class|struct|union|enum|extern|typedef|using|case|default|static_assert|assert'

function Get-SourceFunctions {
    param([string[]]$Lines)

    $n = $Lines.Count
    $code = New-Object string[] $n
    $inBlock = $false
    for ($i = 0; $i -lt $n; $i++) {
        $code[$i] = ConvertTo-CodeLine -Line $Lines[$i] -InBlockComment ([ref]$inBlock)
    }

    $result = New-Object System.Collections.ArrayList
    for ($i = 0; $i -lt $n; $i++) {
        if ($code[$i].Trim() -ne '{') { continue }

        # 向前收集签名，遇到语句边界即停
        $sig = New-Object System.Collections.ArrayList
        $k = $i - 1
        while ($k -ge 0 -and $sig.Count -lt 15) {
            $pt = $code[$k].Trim()
            if ($pt -eq '') { break }
            if ($pt.StartsWith('#')) { break }
            if ($pt.EndsWith(';') -or $pt.EndsWith('{') -or $pt.EndsWith('}')) { break }
            if ($pt -match '^(public|private|protected)\s*:') { break }
            $sig.Insert(0, $pt)
            if ($pt -match '\)\s*[\w\s]*$') { break }
            $k--
        }
        if ($sig.Count -eq 0) { continue }

        $text = ($sig -join ' ').Trim()

        if ($text -match '^template\s*<') {
            $text = ($text -replace '^template\s*<[^<>]*(?:<[^<>]*>[^<>]*)*>\s*', '').Trim()
            if ($text -eq '') { continue }
        }

        if ($text -match "^($script:NonFunctionHeads)\b") { continue }
        if (-not ($text -match '\)\s*[\w\s]*$')) { continue }

        $pIdx = $text.IndexOf('(')
        if ($pIdx -lt 0) { continue }
        if ($text.Substring(0, $pIdx) -match '=') { continue }

        $m = [regex]::Match($text, '(operator\s*[^\s(]+|~?[A-Za-z_]\w*(?:::[~A-Za-z_]\w*)*)\s*\(')
        if (-not $m.Success) { continue }
        $name = ($m.Groups[1].Value -replace '\s+', '')

        # 匹配函数体结束
        $d = 1
        $end = $i
        for ($j = $i + 1; $j -lt $n; $j++) {
            $d += ([regex]::Matches($code[$j], '\{')).Count
            $d -= ([regex]::Matches($code[$j], '\}')).Count
            if ($d -le 0) { $end = $j; break }
        }

        [void]$result.Add([pscustomobject]@{
                Name  = $name
                Start = $i + 1
                End   = $end + 1
            })
    }
    return $result
}

function ConvertTo-SrcRelative {
    param([string]$FileName)
    $idx = $FileName.IndexOf('src\', [System.StringComparison]::OrdinalIgnoreCase)
    if ($idx -ge 0) { return 'src\' + $FileName.Substring($idx + 4) }
    $idx = $FileName.IndexOf('src/', [System.StringComparison]::OrdinalIgnoreCase)
    if ($idx -ge 0) { return 'src/' + $FileName.Substring($idx + 4) }
    return $FileName
}

# 统计覆盖率时跳过的文件（叶子名，支持通配符）——口径对齐 design §6 明确不测清单：
# 「E2E 走不到也不该走到的代码不进分母/未命中清单」。只排**整文件结构性不可达**的：
#   · LuaSyntaxChecker：剧本不触发；
#   · SnoopSystem：仅服务巫师 snoop 命令（§6 并入巫师行），剧本暂无 snoop 面；
#   · MySqlDb：E2E 用 SQLite，MySQL 后端走不到（run-coverage 侧同名排除）；
#   · CreateDump：正常轮不触发崩溃过滤器（run-coverage 侧同名排除）；
#   · AutoFight：§6——vendetta/berserk 无设置者；aggressive 由 persist 段二进房
#     的喽啰 init 触发到分发器一层，策略 call_out 是否执行不稳定，不进分母。
# CmdWizard* 已移出本表（N4 L9 巫师道落地）：巫师命令由 L9 真实执行（full/set_skill/
# call…），整文件排除不再诚实；其未覆盖的巫师函数（reboot/ban/snoop/clone…）进
# 未命中清单保持可见——**不追求覆盖度**（2026-11-01 决策，design §6 巫师行）：
# 巫师命令是服务协议的工具面，N4 引入 L9 的对象是协议本身；服务链自然用到的
# full/set_skill/call 已覆盖属副产品，余 38 条不设消化目标、不计欠账。
# 反向边界（**不排**）：Channel.cpp、main.cpp、HeartBeat.cpp、Logger.cpp、
# CmdUserSocial.cpp、CReward.cpp 只有**个别不测分支**（snoop 查询、托盘 UI、alarm、
# run_id、nullify_vote、victim_penalty），文件主体被 E2E 真实走到——整文件排会剔掉
# 诚实的命中；这些保留分母，其不测性在未命中清单里以 §6 决策认领表达。
$script:CoverageSkipPatterns = @(
    "LuaSyntaxChecker.cpp",   # Lua 语法检查器：剧本不触发
    "SnoopSystem.cpp",        # 巫师 snoop 域（§6 并入巫师行）：整文件仅服务 snoop
    "MySqlDb.cpp",            # MySQL 后端：E2E 用 SQLite（§6）
    "CreateDump.cpp",         # 崩溃过滤器：正常轮不触发（§6，单测口径同名排除）
    "AutoFight.cpp"           # §6：vendetta/berserk 无触发面；persist 段二进房时
                              # 喽啰 init 会调 auto_fight 分发器，但其策略 call_out
                              # 是否执行不稳定，整文件仍按 §6 排除不进分母
)

function Test-CoverageSkippedFile {
    param([string]$FileName)
    $leaf = Split-Path $FileName -Leaf
    foreach ($pat in $script:CoverageSkipPatterns) {
        if ($leaf -like $pat) { return $true }
    }
    return $false
}

function Get-UnhitFunctionReport {
    param([string]$CoberturaPath)

    $srcRoot = Join-Path $RepoRoot 'src'
    if (-not (Test-Path $CoberturaPath)) { throw "找不到 Cobertura 报告：$CoberturaPath" }

    [xml]$x = Get-Content -LiteralPath $CoberturaPath -Encoding UTF8
    $classes = @($x.coverage.packages.package.classes.class)

    $unhit = New-Object System.Collections.ArrayList
    $totalFunctions = 0
    $hitFunctions = 0
    $scannedFiles = 0

    foreach ($c in $classes) {
        $rel = ConvertTo-SrcRelative ([string]$c.filename)
        if ($rel -notmatch '^src[\\/]') { continue }
        if (Test-CoverageSkippedFile $rel) { continue }

        $hits = @{}
        foreach ($ln in @($c.lines.line)) {
            $hits[[int]$ln.number] = [int]$ln.hits
        }

        $filePath = Join-Path $srcRoot ($rel -replace '^src[\\/]', '')
        if (-not (Test-Path -LiteralPath $filePath)) { continue }
        $scannedFiles++

        $lines = [System.IO.File]::ReadAllLines($filePath)
        if ($lines.Count -eq 0) { continue }

        foreach ($fn in (Get-SourceFunctions -Lines $lines)) {
            $anyReported = $false
            $anyHit = $false
            for ($ln = $fn.Start; $ln -le $fn.End; $ln++) {
                if ($hits.ContainsKey($ln)) {
                    $anyReported = $true
                    if ($hits[$ln] -gt 0) { $anyHit = $true; break }
                }
            }
            if (-not $anyReported) { continue }   # 无插桩数据，无法判定
            $totalFunctions++
            if ($anyHit) { $hitFunctions++ }
            else {
                [void]$unhit.Add([pscustomobject]@{
                        File  = $rel
                        Name  = $fn.Name
                        Start = $fn.Start
                        End   = $fn.End
                        Lines = $fn.End - $fn.Start + 1
                    })
            }
        }
    }

    return [pscustomobject]@{
        ScannedFiles   = $scannedFiles
        TotalFunctions = $totalFunctions
        HitFunctions   = $hitFunctions
        UnhitFunctions = $unhit
    }
}

# ===========================================================================
# 辅助
# ===========================================================================

function Get-IniValue([string]$path, [string]$section, [string]$key) {
    $cur = ""
    foreach ($line in [System.IO.File]::ReadAllLines($path, [System.Text.Encoding]::UTF8)) {
        $t = $line.Trim()
        if ($t -match '^\[(.+)\]$') { $cur = $Matches[1]; continue }
        if ($cur -eq $section -and $t -match "^$key\s*=\s*(.*)$") {
            return $Matches[1].Trim()
        }
    }
    return ""
}

function Get-MSBuildPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $p = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" 2>$null
        if ($p) { return @($p)[0].Trim() }
    }
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw "找不到 MSBuild（请安装 Visual Studio 或把 msbuild.exe 加入 PATH）"
}

function Wait-Port([int]$p, [int]$timeoutSec) {
    $start = Get-Date
    $nextBeat = $start.AddSeconds($HeartbeatSec)
    $deadline = $start.AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        if ((Test-NetConnection -ComputerName 127.0.0.1 -Port $p -WarningAction SilentlyContinue).TcpTestSucceeded) {
            return $true
        }
        $now = Get-Date
        if ($now -ge $nextBeat) {
            Write-Host ("   ... 等待端口 $p 就绪（已 {0}s，上限 {1}s）" -f `
                    [int]($now - $start).TotalSeconds, $timeoutSec)
            $nextBeat = $now.AddSeconds($HeartbeatSec)
        }
        Start-Sleep -Milliseconds 300
    }
    return $false
}

# 定位「本次测试的服务器」——**只按测试端口认进程，绝不按进程名全服清场**。
# 有人会用同一个 bin\jianghu.exe 在别的端口（data\config.ini 默认 6600）做功能测试，
# 按进程名 Get-Process jianghu 杀会把那台服务器误杀。
# -NoCoverage 下服务器由本脚本直接拉起，句柄最可靠；覆盖率模式下服务器是
# OpenCppCoverage 的子进程、本脚本没有句柄，改用「监听测试端口的 jianghu 进程」定位。
function Get-TestServerProcess {
    param([int]$Port)

    if ($null -ne $serverProc -and -not $serverProc.HasExited) { return $serverProc }

    $conn = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $conn) { return $null }
    $p = Get-Process -Id $conn.OwningProcess -ErrorAction SilentlyContinue
    if ($null -ne $p -and $p.ProcessName -eq 'jianghu') { return $p }
    return $null
}

function Stop-ServerGraceful {
    param([int]$Port)

    $p = Get-TestServerProcess -Port $Port
    if ($null -eq $p) { return }
    $serverPid = $p.Id
    try { [void]$p.CloseMainWindow() } catch { }
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        # 按 pid 等，不能按进程名等：按名等时别的 jianghu 还在，
        # 会白等满 30 秒、再把它和测试服务器一起强杀
        if (-not (Get-Process -Id $serverPid -ErrorAction SilentlyContinue)) { return }
        Start-Sleep -Milliseconds 500
    }
    Get-Process -Id $serverPid -ErrorAction SilentlyContinue | Stop-Process -Force
}

# 启动前清场：只回收「上一次运行残留、仍监听测试端口」的测试服务器。
# 端口被别的程序占用时直接报错，而不是连它一起杀（否则 Wait-Port 会误判就绪，
# 后续 CLI 全连到错误的进程上）。
function Stop-OrphanTestServer {
    param([int]$Port)

    $owningPids = @(Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty OwningProcess -Unique)
    foreach ($procId in $owningPids) {
        $p = Get-Process -Id $procId -ErrorAction SilentlyContinue
        if ($null -eq $p) { continue }
        if ($p.ProcessName -ne 'jianghu') {
            throw "端口 $Port 被其他进程占用：$($p.ProcessName)（pid $procId），请先释放该端口（本脚本只回收自己的测试服务器）。"
        }
        Write-Host "-- 回收上次运行残留的测试服务器（pid $procId，端口 $Port）"
        Stop-Process -Id $procId -Force
    }
}

# 启动主套件「被测服务器」（单实例）：-NoCoverage 下由本脚本直接拉起；
# 覆盖率模式经 OpenCppCoverage 拉起、只导二进制 e2e_sample_main.cov——
# 最终 html/cobertura 由末尾的**合并通行证**统一产出（主服 + persist 试验台
# 各段三路 --input_coverage 合并；OCC 启动时校验输入文件存在，故只能等
# 全部实例导出完毕后再跑这一趟）。
function Start-E2EServer {
    if ($NoCoverage) {
        $script:serverRun = Start-HiddenProcess -Exe $ServerExe -WorkingDirectory $RepoRoot `
            -Arguments @("--config", $ConfPath, "--log-run-id", $LogRunId)
        $script:serverProc = $script:serverRun.Process
        return
    }

    if (Test-Path $ReportDir) { Remove-Item $ReportDir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path (Split-Path $CovBin) | Out-Null
    $occArgs = @(
        "--modules", $ServerExe,
        "--sources", (Join-Path $RepoRoot "src"),
        "--optimized_build",
        "--working_dir", $RepoRoot,
        "--export_type", "binary:$CovBinMain",
        "--",
        $ServerExe, "--config", $ConfPath, "--log-run-id", $LogRunId
    )
    $script:occRun = Start-HiddenProcess -Exe $OccExe -WorkingDirectory $RepoRoot -Arguments $occArgs
    Write-Host "-- OpenCppCoverage 已启动（pid $($occRun.Process.Id)，无窗口）"
}

# 等待 OpenCppCoverage 进程退出并导出二进制覆盖率，返回等待耗时（秒）。
# 采集日志留档，便于排查报告异常。
function Wait-CoverageExport {
    param($OccRun, [string]$LogName)

    Write-Host "-- 等待 OpenCppCoverage 导出报告..."
    $occProc = $OccRun.Process
    $occStart = Get-Date
    $occDeadline = $occStart.AddSeconds(180)
    $occBeat = $occStart.AddSeconds(30)
    while ((Get-Date) -lt $occDeadline) {
        if ($occProc.HasExited) { break }
        $now = Get-Date
        if ($now -ge $occBeat) {
            $occBeat = $now.AddSeconds(30)
            Write-Host ("   ... 服务器已停止，等待导出报告（已 {0}s，上限 180s）" -f `
                    [int]($now - $occStart).TotalSeconds)
        }
        Start-Sleep -Milliseconds 500
    }
    if (-not $occProc.HasExited) {
        Write-Warning "OpenCppCoverage 未退出，强制结束"
        $occProc | Stop-Process -Force
    }
    else {
        # 采集日志留档，便于排查报告异常
        $occLog = Join-Path (Split-Path $CovBin) $LogName
        @($OccRun.StdOut.Result, $OccRun.StdErr.Result) |
            Where-Object { $_ } | Set-Content -Encoding UTF8 $occLog
    }
    return [int]((Get-Date) - $occStart).TotalSeconds
}

# 给子进程参数加引号（Start-Process / ProcessStartInfo.Arguments 只接受单个字符串）
function Quote-Arg([string]$s) {
    if ($s -match '[\s"]') { return '"' + ($s -replace '"', '\"') + '"' }
    return $s
}

# 无窗口启动控制台子进程，避免弹出/抢占前台的控制台窗口。
# CreateNoWindow = true 会带上 CREATE_NO_WINDOW，控制台程序也不会分配新控制台。
function New-HiddenStartInfo([string]$Exe, [string[]]$Arguments, [string]$WorkingDirectory) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = (($Arguments | ForEach-Object { Quote-Arg $_ }) -join ' ')
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    # 子进程（CLI / OpenCppCoverage）输出 UTF-8 字节；重定向流默认按
    # [Console]::OutputEncoding（本机多为 cp936）解码，会导致中文乱码。
    # 这里按流固定为 UTF-8，与 [Console]::OutputEncoding 解耦，避免影响 Write-Host。
    $psi.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $psi.StandardErrorEncoding = [System.Text.Encoding]::UTF8
    return $psi
}

# 无窗口启动长驻子进程（用于 OpenCppCoverage）；输出由调用方在退出后取回
function Start-HiddenProcess {
    param([string]$Exe, [string[]]$Arguments, [string]$WorkingDirectory)

    $psi = New-HiddenStartInfo $Exe $Arguments $WorkingDirectory
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true

    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    [void]$p.Start()
    return [pscustomobject]@{
        Process = $p
        StdOut  = $p.StandardOutput.ReadToEndAsync()
        StdErr  = $p.StandardError.ReadToEndAsync()
    }
}

# ===========================================================================
# 准备
# ===========================================================================

foreach ($p in @($ConfPath, $CliConf, $SideConfPath, $SideCliConf,
        $ReloadConfPath, $ReloadCliConf, $ReloadTestbedScript, $ServerProj)) {
    if (-not (Test-Path $p)) { throw "缺少文件：$p" }
}

$Port = [int](Get-IniValue $ConfPath "server" "port")
# persist 试验台：独立端口 / 独立库（配置见 tests\integration\config.persist.ini）
$SidePort = [int](Get-IniValue $SideConfPath "server" "port")
if ($SidePort -eq $Port) { throw "$SideConfPath 的 [server] port 必须与主配置错开（当前都是 $Port）" }
# reload 试验台：独立端口 / 独立库 / 私有脚本副本（config.reload.ini）
$ReloadPort = [int](Get-IniValue $ReloadConfPath "server" "port")
if ($ReloadPort -eq $Port -or $ReloadPort -eq $SidePort) {
    throw "$ReloadConfPath 的 [server] port 必须与主配置（$Port）及 persist 试验台（$SidePort）都错开，当前：$ReloadPort"
}
$DbType = Get-IniValue $ConfPath "database" "type"
if ($DbType -ne "sqlite") { throw "$ConfPath 的 [database] type 必须为 sqlite（E2E 已不依赖外部数据库服务），当前：'$DbType'" }
$DbPath = Get-IniValue $ConfPath "database" "path"
if ($DbPath -eq "") { throw "无法从 $ConfPath 解析 [database] path" }
# config 里的 path 相对仓库根（服务器以仓库根为工作目录启动）；脚本侧拼成绝对路径
if (-not [System.IO.Path]::IsPathRooted($DbPath)) { $DbPath = Join-Path $RepoRoot ($DbPath -replace "/", "\") }
$SideDbPath = Get-IniValue $SideConfPath "database" "path"
if ($SideDbPath -eq "") { throw "无法从 $SideConfPath 解析 [database] path" }
if (-not [System.IO.Path]::IsPathRooted($SideDbPath)) { $SideDbPath = Join-Path $RepoRoot ($SideDbPath -replace "/", "\") }
if ($CliExe -eq "") { $CliExe = Join-Path $RepoRoot "bin\cli\jianghu_cli.exe" }

if ($Simple) {
    Write-Host "== lua-sample 集成测试 · 简单模式（冒烟：网络联通 / 行走 / look）=="
    Write-Warning "-Simple 仅供人类快速确认；AI/Agent 禁用——AI 的验证与交付验收必须跑完整模式。"
}
else {
    Write-Host "== lua-sample 集成测试（单人主线 E2E）=="
}
Write-Host "repo     : $RepoRoot"
Write-Host "config   : $ConfPath"
Write-Host "port     : $Port   db: sqlite $DbPath"

# --- 构建 -------------------------------------------------------------------
if (-not $SkipBuild) {
    $msb = Get-MSBuildPath
    Write-Host "-- 构建服务器与 CLI（Release|Win32）"
    $t0 = Get-Date
    # CLI 正常输出到 bin\cli\；服务器单独输出到 bin\e2e\（见上 $E2EDir 注释），
    # 使集成测试写自己的 exe，不与正在运行的 bin\jianghu.exe 抢文件锁（LNK1104）。
    # 注意用双写结尾反斜杠：既满足 OutDir 必须以 \ 结尾，又避开 PowerShell 向
    # 原生命令传「单个结尾反斜杠」被吃掉的坑。
    & $msb $CliProj /p:Configuration=Release /p:Platform=Win32 /m /nologo /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw "构建失败：$CliProj" }
    New-Item -ItemType Directory -Force -Path $E2EDir | Out-Null
    & $msb $ServerProj /p:Configuration=Release /p:Platform=Win32 /m "/p:OutDir=$E2EDir\\" /nologo /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw "构建失败：$ServerProj" }
    # 虽然 E2E 数据库走 SQLite（sqlite3 静态编入），但 jianghu.exe 仍隐式链接
    # libmysql.lib（非 delay-load），进程启动时加载器就要求 libmySQL.dll 在场——
    # 故 bin\*.dll 仍随 exe 带到 bin\e2e\。只在目标缺失时复制：bin\e2e\ 可能残留
    # 上一次未退净的服务器（占着 DLL，脚本稍后在服务启动阶段按测试端口定向回收），
    # 此时 DLL 已在、无需强写。
    foreach ($dll in Get-ChildItem (Join-Path $RepoRoot "bin\*.dll") -ErrorAction SilentlyContinue) {
        $dst = Join-Path $E2EDir $dll.Name
        if (-not (Test-Path $dst)) {
            Copy-Item -Destination $dst -ErrorAction SilentlyContinue
        }
    }
    $tBuildSec = [int]((Get-Date) - $t0).TotalSeconds
}

foreach ($p in @($ServerExe, $CliExe)) {
    if (-not (Test-Path $p)) { throw "缺少文件：$p（可去掉 -SkipBuild 重新构建）" }
}
if (-not $NoCoverage -and -not (Test-Path $OccExe)) {
    throw "找不到 OpenCppCoverage：$OccExe（可用 -NoCoverage 跳过）"
}

# 整段墙钟起算点（简单模式预算口径）：从这里开始的一切都算进 30s——重建库、
# 生成 vars、回收残留服务器、起服、注册、各道、关服与观测收尾；只有构建在外。
$tWallStart = Get-Date

# --- 生成各道的 vars（唯一角色名 + 邮箱） --------------------------------------
# 角色名长度限制为 2~10 字节（LoginRegister::check_legal_name / make_body）：
# "e2e"(3) + 6 位十六进制(6) + 1 位道序号(1) = 10 字节。同轮内各道靠末位道序号
# 互不重名，跨轮靠 6 位时间片段避免与历史账号撞名（重名会注册失败）。
$RunHex = '{0:x6}' -f ([DateTime]::UtcNow.Ticks -band 0xFFFFFF)
# L9 巫师道密码：取 config [wizard] password（缺省 12345678，与服务端
# Sql.cpp ensure_wizard_accounts 同缺省），只在 config 有 [wizard] 节时读它
$wizardPass = "12345678"
$inWizardSection = $false
foreach ($cfgLine in Get-Content -LiteralPath $ConfPath) {
    if ($cfgLine -match '^\s*\[([^\]]+)\]') { $inWizardSection = ($Matches[1] -eq 'wizard') }
    elseif ($inWizardSection -and $cfgLine -match '^\s*password\s*=\s*(.+?)\s*$') { $wizardPass = $Matches[1] }
}
foreach ($lane in $ActiveLanes) {
    if (-not $lane.NoRegister) {
        $lane.Name = "e2e$RunHex$($lane.Id)"
        $lane.Email = "e2e$RunHex$($lane.Id)@example.com"
    }
    else {
        # 巫师道：角色名固定「巫师一号」（服务端自举），无注册流程
        $lane.Name = "巫师一号"
    }
    $lane.RegReport = Join-Path $WorkDir ("lane{0}.register.report" -f $lane.Id)
    $lane.Status = "fail"
    $lane.Steps = 0
    $lane.Reason = ""
    $lane.Result = $null
    $gender = if ($lane.Gender) { $lane.Gender } else { "男性" }
    $attrs = if ($lane.Attrs) { $lane.Attrs } else { "21 21 21 21" }
    # 无 BOM UTF-8：gender 含中文（女性），ASCII 会损坏为 ??；BOM 会毁掉首行
    # key（CLI load_vars 按原始字节切 '='，BOM 拼进 "name" 使 $name 替换失效）。
    # 后续 peer/peer_acct 追加是纯 ASCII，与本文件字节兼容；CLI save_vars 重写
    # 时同样不带 BOM，全链编码一致。
    # NoRegister（L9 巫师道）：账号自举，vars 直写 acct=1 + 巫师密码，
    # 剧本以 common/login.sce 的 login $acct $pass 直接登录。
    $varsText = if ($lane.NoRegister) {
        "acct=1`npass=$wizardPass`n"
    }
    else {
        "name=$($lane.Name)`npass=e2e1234`nemail=$($lane.Email)`ngender=$gender`nattrs=$attrs`n"
    }
    [System.IO.File]::WriteAllText($lane.VarsFile, $varsText,
        (New-Object System.Text.UTF8Encoding($false)))
}

# 配对互动道 peer 注入：把对方角色名写进本道 vars（peer 键），剧本以 $peer 引用。
# 角色名在此阶段已全部确定（上一循环），注册阶段 CLI 会原样保留 vars 里已有的键
# （CScenarioRunner 的 load/save 只增 acct 等产出键，不动输入键），故无需改 CLI。
foreach ($lane in $ActiveLanes) {
    if (-not $lane.PeerId) { continue }
    $peerLane = @($ActiveLanes | Where-Object { $_.Id -eq $lane.PeerId }) | Select-Object -First 1
    Add-Content -Encoding ASCII $lane.VarsFile "peer=$($peerLane.Name)"
}

# 观战道 spectator 注入：把 L7 角色名写进依赖道（L5）的 vars（spectator 键），
# 与 peer 同机制同阶段；剧本以 $spectator / $spectator_acct 做跨房开战信号。
foreach ($lane in $ActiveLanes) {
    if (-not $lane.WitnessId) { continue }
    $witLane = @($ActiveLanes | Where-Object { $_.Id -eq $lane.WitnessId }) | Select-Object -First 1
    Add-Content -Encoding ASCII $lane.VarsFile "spectator=$($witLane.Name)"
}

# ===========================================================================
# 运行
# ===========================================================================

$serverProc = $null
$serverRun = $null
$occRun = $null
$status = "fail"
$failReason = ""
# 崩溃检测基线：本次运行启动服务器前仓库根已有的 dump 文件集合。
# $null 表示快照没做成（服务器未启动），此时跳过崩溃检测。
$dumpsBefore = $null
# 服务端日志观测基线：启动前 tests/integration/log\ 各日志文件的字节偏移。
# 停服后只扫本次新增区间，避免把上一轮的错误算进本轮。
# $null = 快照没做成（服务器未启动），跳过扫描——同 $dumpsBefore 的语义。
$serverLogOffsets = $null
$ServerLogDir = Join-Path $RepoRoot "tests\integration\log"
$LogAllowFile = Join-Path $IntDir "log-allowlist.txt"
# 本轮服务端日志标识：传给 jianghu.exe --log-run-id，本轮日志单独落在
# <log 路径>/M-D-<id>.txt（文件名规则见 Logger.cpp）。扫描口径不变（仍按字节
# 偏移扫全目录增量，跨轮错误不算进本轮），run id 的作用是在报告里点名本轮文件，
# 排查时不必翻目录猜「哪份是这次的」。毫秒时间戳避免同秒并行两轮撞同一文件名。
$LogRunId = "e2e-" + (Get-Date -Format "yyyyMMdd-HHmmss-fff")
# 本轮日志文件路径行（finally 里产出，$lines 创建后并入报告）。
$srvLogFileLine = $null
# 服务端日志扫描的 summary 行（finally 里产出，$lines 创建后并入报告）。
$srvLogSummary = $null
# 旧 dump 归档目录（仓库根 crash_dumps\）：启动前把上一轮遗留 dump 挪进去；
# 本轮新产生的 dump 绝不挪动，原地留在仓库根供查看。
$OldDumpArchive = Join-Path $RepoRoot "crash_dumps"

# 事件 action 跑在独立 runspace：只把行塞进本道自己的共享队列，不直接刷屏——
# 多道并行时逐行打印会互相穿插淹没控制台。进度改由各道心跳汇报，完整输出落盘
# 到各道的 .log，任一道失败时再把它的尾部打出来。退出码与剧本报告（--report）
# 都不依赖这条输出流，故即使事件没及时回来，判定结果依然可靠。
$script:EmitLaneLine = {
    $line = $EventArgs.Data
    if ($null -eq $line -or $line -eq '') { return }
    $md = $Event.MessageData
    $md.Queue.Enqueue($line)
    $md.State.Last = $line
    $md.State.Count = [int]$md.State.Count + 1
}

# 并行跑一批无头剧本：每项开一个 CLI 进程同时在跑，轮询给每项打心跳，
# 全部退出后收集输出、写日志、解析 PASS 步数。
# 输入每项：Tag / Arguments[] / LogFile；返回项附带 ExitCode / Output / Steps。
function Invoke-ParallelBatch {
    param(
        [Parameter(Mandatory)] [hashtable[]]$Items,
        [Parameter(Mandatory)] [string]$Phase
    )

    $entries = New-Object System.Collections.ArrayList
    $completed = $false
    try {
        foreach ($it in $Items) {
            $psi = New-HiddenStartInfo -Exe $CliExe -WorkingDirectory $RepoRoot -Arguments $it.Arguments
            $psi.RedirectStandardOutput = $true
            $psi.RedirectStandardError = $true
            $p = New-Object System.Diagnostics.Process
            $p.StartInfo = $psi
            [void]$p.Start()

            $state = [hashtable]::Synchronized(@{ Last = ""; Count = 0 })
            $shared = @{
                Queue = [System.Collections.Concurrent.ConcurrentQueue[string]]::new()
                State = $state
            }
            $outName = "e2e-$Phase-$($it.Tag)-out"
            $errName = "e2e-$Phase-$($it.Tag)-err"
            $ev = @()
            $ev += Register-ObjectEvent -InputObject $p -EventName OutputDataReceived `
                -SourceIdentifier $outName -MessageData $shared -Action $script:EmitLaneLine
            $ev += Register-ObjectEvent -InputObject $p -EventName ErrorDataReceived `
                -SourceIdentifier $errName -MessageData $shared -Action $script:EmitLaneLine
            $p.BeginOutputReadLine()
            $p.BeginErrorReadLine()

            [void]$entries.Add([pscustomobject]@{
                    Tag      = $it.Tag
                    Proc     = $p
                    Queue    = $shared.Queue
                    State    = $state
                    OutName  = $outName
                    ErrName  = $errName
                    Events   = $ev
                    LogFile  = $it.LogFile
                    Started  = Get-Date
                    DurationSec = 0
                    ExitCode = -1
                    Output   = @()
                    Steps    = 0
                })
        }

        # 轮询心跳直到全部退出；每 HeartbeatSec 给每道报一次状态
        $nextBeat = (Get-Date).AddSeconds($HeartbeatSec)
        while ($true) {
            $pending = @($entries | Where-Object { -not $_.Proc.HasExited })
            if ($pending.Count -eq 0) { break }

            $now = Get-Date
            if ($now -ge $nextBeat) {
                $nextBeat = $now.AddSeconds($HeartbeatSec)
                foreach ($e in $entries) {
                    if ($e.Proc.HasExited) { continue }
                    $last = [string]$e.State.Last
                    if ($last -eq '') { $last = "（尚无输出）" }
                    if ($last.Length -gt 44) { $last = $last.Substring(0, 44) + "..." }
                    Write-Host ("   [{0}] {1,3}s 收 {2,4} 行 | {3}" -f `
                            $e.Tag, [int]($now - $e.Started).TotalSeconds, [int]$e.State.Count, $last)
                }
            }
            Start-Sleep -Milliseconds 300
        }

        # 等异步读取收尾，保证输出队列完整；再取退出码、落日志、解析步数
        foreach ($e in $entries) {
            $e.Proc.WaitForExit()
            # 单道耗时必须用进程真实退出时刻：本循环在全部进程退出后才执行，
            # 若在此处取 Get-Date，所有道都会记成批总时长（关键路径分析会失真）。
            try {
                $e.DurationSec = [Math]::Max(0, [int]($e.Proc.ExitTime - $e.Started).TotalSeconds)
            }
            catch {
                $e.DurationSec = [Math]::Max(0, [int]((Get-Date) - $e.Started).TotalSeconds)
            }
            $e.ExitCode = $e.Proc.ExitCode

            $out = @()
            $line = ""
            while ($e.Queue.TryDequeue([ref]$line)) { $out += $line }
            $e.Output = $out
            $out | Set-Content -Encoding UTF8 $e.LogFile

            $m = [regex]::Match(($out -join "`n"), '\[scenario\] PASS:\s*(\d+)\s*steps')
            if ($m.Success) { $e.Steps = [int]$m.Groups[1].Value }
        }
        $completed = $true
    }
    finally {
        foreach ($e in $entries) {
            foreach ($name in @($e.OutName, $e.ErrName)) {
                Unregister-Event -SourceIdentifier $name -ErrorAction SilentlyContinue
            }
            foreach ($j in $e.Events) { Remove-Job -Job $j -Force -ErrorAction SilentlyContinue }
            if (-not $completed) {
                # 异常路径上不留孤儿 CLI，否则它会占着测试角色继续跑
                try { if (-not $e.Proc.HasExited) { $e.Proc.Kill() } } catch { }
            }
            $e.Proc.Dispose()
        }
    }
    return $entries
}

# 打印某道输出的尾部（失败时用，定位失败步骤）
function Show-LaneTail($result, [int]$lines) {
    if ($null -eq $result) { return }
    $tail = @($result.Output | Select-Object -Last $lines)
    foreach ($l in $tail) { Write-Host "        $l" }
}

# ===========================================================================
# 服务端日志观测（轨道 L）
# ===========================================================================
# 剧本断言的是「客户端收到了什么」；服务端自己报的错是另一双眼睛——Lua 异常、
# 命令处理失败、DB 错误都只落在服务端日志里。这里在停服后扫本次运行新增的
# 日志区间：
#   · 未登记的 ERROR/FATAL = 本轮引入的服务端侧故障 → 判 FAIL（与崩溃 dump
#     检测同级：剧本全绿但服务端报错同样是矛盾信号）；
#   · 已登记的（故意负测等预期产物）静默，签名登记在 tests/integration/
#     log-allowlist.txt（每行：签名<TAB>归属场景/理由，# 起头为注释）；
#   · WARN 仅按函数聚合计数入报告，不判失败（地图边界类告警噪音大）。
# 签名匹配对「去掉 [LEVEL] (时间戳) 前缀后的消息体」做子串匹配：签名应带
# 变量锚点（如 wrongpass、house_），既覆盖该类预期、又不放过同类中的真异常。
function Get-ServerLogIssues {
    param([hashtable]$Offsets, [string]$AllowFile)

    $allow = @()
    if (Test-Path -LiteralPath $AllowFile) {
        foreach ($line in (Get-Content -LiteralPath $AllowFile -Encoding UTF8)) {
            $t = $line.Trim()
            if ($t -eq '' -or $t.StartsWith('#')) { continue }
            $sig = ($t -split "`t", 2)[0].Trim()
            if ($sig -ne '') { $allow += $sig }
        }
    }

    $unknown = New-Object System.Collections.ArrayList
    $warnByFunc = @{}
    $errTotal = 0
    foreach ($f in @(Get-ChildItem -LiteralPath $ServerLogDir -Filter "*.txt" -File -ErrorAction SilentlyContinue)) {
        $start = 0
        if ($Offsets.ContainsKey($f.Name)) { $start = [long]$Offsets[$f.Name] }
        if ($f.Length -le $start) { continue }
        # 文件被服务器以 append 模式写、只读共享打开：用 FileStream 按偏移读增量，
        # 不把整个历史文件读进来（跨天旧错误不算进本轮）。
        $fs = [System.IO.File]::Open($f.FullName, 'Open', 'Read', 'ReadWrite')
        try {
            $null = $fs.Seek($start, 'Begin')
            $sr = New-Object System.IO.StreamReader($fs, [System.Text.Encoding]::UTF8)
            $content = $sr.ReadToEnd()
            $sr.Close()
        }
        finally { $fs.Dispose() }

        foreach ($line in ($content -split "`r?`n")) {
            if ($line -match '^\[(ERROR|FATAL)\s*\]') {
                $errTotal++
                $msg = ($line -replace '^\[\w+\s*\]\s*\([^)]*\)\s*', '')
                $hit = $false
                foreach ($sig in $allow) { if ($msg.Contains($sig)) { $hit = $true; break } }
                if (-not $hit) { [void]$unknown.Add($line) }
            }
            elseif ($line -match '^\[WARN\s*\]') {
                $m = [regex]::Match($line, '^\[WARN\s*\]\s*\([^)]*\)\s*(\w+):')
                $fn = if ($m.Success) { $m.Groups[1].Value } else { '?' }
                $warnByFunc[$fn] = [int]$warnByFunc[$fn] + 1
            }
        }
    }
    return [pscustomobject]@{
        Unknown  = @($unknown)
        ErrTotal = $errTotal
        WarnByFn = $warnByFunc
    }
}

# ===========================================================================
# 耗时监控（文首第 8 条）
# ===========================================================================

# 递归展开剧本的 include，返回相对 scenarios 目录的场景文件清单（'/' 分隔、字典序）。
# 展开口径与 CLI CScenarioRunner::load_script 一致：相对路径基于包含文件所在目录。
# include 参数含 $var 时无法静态展开，按字面量记录（当前剧本无此用法）。
function Get-LaneScenarioList {
    param([Parameter(Mandatory)][string]$LaneScript)

    $root = $ScenarioDir
    $result = New-Object System.Collections.ArrayList
    $seen = @{}
    $stack = New-Object System.Collections.Stack
    $stack.Push((Join-Path $root $LaneScript))
    while ($stack.Count -gt 0) {
        $path = [string]$stack.Pop()
        if ($seen.ContainsKey($path)) { continue }
        $seen[$path] = $true

        if ($path.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
            $rel = $path.Substring($root.Length).TrimStart('\', '/')
        }
        else { $rel = $path }
        [void]$result.Add(($rel -replace '\\', '/'))

        if (-not (Test-Path -LiteralPath $path)) { continue }
        $baseDir = Split-Path $path -Parent
        foreach ($line in (Get-Content -LiteralPath $path -Encoding UTF8)) {
            $t = $line.Trim()
            if ($t -eq '' -or $t.StartsWith('#')) { continue }
            $m = [regex]::Match($t, '^include\s+(\S+)')
            if (-not $m.Success) { continue }
            $inc = $m.Groups[1].Value
            if ($inc -match '^(?:[A-Za-z]:[\\/]|[\\/])') { $stack.Push($inc) }
            else { $stack.Push((Join-Path $baseDir $inc)) }
        }
    }
    return @($result | Sort-Object)
}

# 读取耗时历史（JSON Lines，一行一次 PASS 运行）；坏行跳过。
function Read-TimingHistory {
    if (-not (Test-Path -LiteralPath $TimingHistoryFile)) { return @() }
    $list = New-Object System.Collections.ArrayList
    foreach ($line in (Get-Content -LiteralPath $TimingHistoryFile -Encoding UTF8)) {
        $t = $line.Trim()
        if ($t -eq '') { continue }
        try { [void]$list.Add(($t | ConvertFrom-Json)) } catch { }
    }
    return @($list)
}

# 追加本次记录；超出 $TimingKeepCount 条时整体裁剪（只保留最近 N 条）。
function Add-TimingRecord {
    param([Parameter(Mandatory)]$Record)

    New-Item -ItemType Directory -Force -Path (Split-Path $TimingHistoryFile -Parent) | Out-Null
    Add-Content -LiteralPath $TimingHistoryFile `
        -Value (ConvertTo-Json -InputObject $Record -Compress -Depth 6) -Encoding UTF8

    $all = @(Read-TimingHistory)
    if ($all.Count -gt $TimingKeepCount) {
        $kept = @($all | Select-Object -Last $TimingKeepCount |
            ForEach-Object { ConvertTo-Json -InputObject $_ -Compress -Depth 6 })
        Set-Content -LiteralPath $TimingHistoryFile -Value $kept -Encoding UTF8
    }
}

# 与前次同类（同模式 / 同 workers / 同覆盖率开关 / 同 dbg / 同 step-timeout 且
# PASS）运行对比，生成报告行：总耗时与阶段构成 → 各道耗时增量 → 新增场景 diff →
# 关键路径 → 拆分建议。拆分收益按「关键路径道的增量 ≈ 新增场景成本、墙钟 = 注册
# + 最长道」估算。
#
# 预算口径选择：完整模式量「剧本执行」（注册 + 各道并行，与关键路径/吸收能力
# 同口径）；简单模式量「整段墙钟」——该模式的存在意义就是「一次运行有多快」，
# 起服与关服正是它要约束的耗时，只量剧本执行会把这部分开销藏在预算之外。
function Get-BudgetMeasure {
    param([Parameter(Mandatory)]$Rec)

    if ([string]$Rec.budget_base -eq 'wall' -and $null -ne $Rec.wall_sec) {
        return @{ Sec = [int]$Rec.wall_sec; Label = "整段墙钟" }
    }
    return @{ Sec = [int]$Rec.exec_sec; Label = "剧本执行总耗时" }
}

function Get-TimingReport {
    param(
        [Parameter(Mandatory)]$Cur,
        $Prev,
        [Parameter(Mandatory)][int]$ThresholdSec,
        [Parameter(Mandatory)][int]$BudgetSec
    )

    $out = New-Object System.Collections.ArrayList
    $regressed = $false
    $causedByNew = $false
    $overBudget = $false

    $regCur = [int]$Cur.reg_sec
    $laneCur = [int]$Cur.lane_sec
    $curExec = [int]$Cur.exec_sec
    $buildTxt = if ($null -ne $Cur.build_sec) { "$($Cur.build_sec)s" } else { "-" }
    $readyTxt = if ($null -ne $Cur.server_ready_sec) { "$($Cur.server_ready_sec)s" } else { "-" }
    $exportTxt = if ($null -ne $Cur.export_sec) { "$($Cur.export_sec)s" } else { "-" }
    # 构成行只含 exec 关键路径（persist 试验台与各道并行、不占关键路径，另起一行）
    $compTxt = "注册 {0}s + 各道并行 {1}s" -f $regCur, $laneCur
    [void]$out.Add(("本次总耗时（剧本执行）: {0}s = {1}" -f $curExec, $compTxt))
    if ($null -ne $Cur.persist_sec) {
        [void]$out.Add(("  persist 试验台（与各道并行）: 段 {0}s + 重启 {1}s（不计入执行总耗时）" -f `
                [int]$Cur.persist_sec, [int]$Cur.restart_sec))
    }
    [void]$out.Add(("  构建 {0} ｜ 起服 {1} ｜ 覆盖率导出 {2}（不计入对比）" -f $buildTxt, $readyTxt, $exportTxt))

    # --- 绝对预算三行硬指标（预算是唯一的通过标尺；Δ 归因在其后，纯诊断） ---
    $meas = Get-BudgetMeasure -Rec $Cur
    $headroom = $BudgetSec - $meas.Sec
    $overBudget = $meas.Sec -gt $BudgetSec
    [void]$out.Add(("预算 {0}s / 实测 {1}s / 余量 {2}{3}s" -f $BudgetSec, $meas.Sec,
            $(if ($headroom -ge 0) { "+" } else { "" }), $headroom))
    if ([string]$Cur.budget_base -eq 'wall') {
        [void]$out.Add("（简单模式预算口径=整段墙钟：构建之后起算，至关服与观测收尾，不含覆盖率导出）")
    }

    # 关键路径与非关键路径吸收能力：墙钟 = 注册 + 最长道，把「加片段往哪放」
    # 的余量显式化——新增场景只要不落关键路径，吸收能力内加满也不动墙钟。
    $critLane = @($Cur.lanes | Sort-Object { -[int]$_.sec }) | Select-Object -First 1
    $otherMax = 0
    foreach ($l in @($Cur.lanes)) {
        if ([int]$l.id -ne [int]$critLane.id -and [int]$l.sec -gt $otherMax) {
            $otherMax = [int]$l.sec
        }
    }
    [void]$out.Add(("关键路径: L{0} {1} {2}s ｜ 次长道 {3}s ｜ 非关键路径可承接约 {4}s 新增而不改墙钟" -f `
            $critLane.id, $critLane.desc, $critLane.sec, $otherMax,
            ([int]$critLane.sec - $otherMax)))

    if ($null -eq $Prev) {
        [void]$out.Add("无同类基线（首次运行或模式/workers/覆盖率/dbg/step-timeout 变更），本次已记为基线。")
        return [pscustomobject]@{ Lines = @($out); Regression = $false; CausedByNewScenario = $false; OverBudget = $overBudget }
    }

    $regPrev = [int]$Prev.reg_sec
    $lanePrev = [int]$Prev.lane_sec
    $prevExec = [int]$Prev.exec_sec
    $delta = $curExec - $prevExec
    $sign = if ($delta -ge 0) { "+" } else { "" }

    [void]$out.Add(("前次总耗时（{0}，同配置）: {1}s = 注册 {2}s + 各道并行 {3}s" -f `
            $Prev.time, $prevExec, $regPrev, $lanePrev))
    [void]$out.Add(("Δ 总耗时（诊断，非门禁）: {0}{1}s（参考阈值 {2}s）" -f $sign, $delta, $ThresholdSec))

    if ($delta -le $ThresholdSec) {
        [void]$out.Add("Δ 未超过参考阈值，无需归因。")
        return [pscustomobject]@{ Lines = @($out); Regression = $false; CausedByNewScenario = $false; OverBudget = $overBudget }
    }
    $regressed = $true

    # --- 各道耗时增量（按耗时降序，首个即当前关键路径） -----------------------
    $prevLanes = @{}
    foreach ($pl in @($Prev.lanes)) { $prevLanes[[int]$pl.id] = $pl }
    $crit = $null
    foreach ($l in (@($Cur.lanes) | Sort-Object { -[int]$_.sec })) {
        $p = $null
        if ($prevLanes.ContainsKey([int]$l.id)) { $p = $prevLanes[[int]$l.id] }
        $secTxt = "{0}s" -f [int]$l.sec
        if ($null -ne $p) {
            $ld = [int]$l.sec - [int]$p.sec
            $secTxt += "（{0}{1}s）" -f $(if ($ld -ge 0) { "+" } else { "" }), $ld
        }
        else { $secTxt += "（前次无此道）" }
        [void]$out.Add(("  L{0} {1,-14} {2}  steps {3}  场景 {4}" -f `
                $l.id, $l.desc, $secTxt, $l.steps, @($l.scenarios).Count))
        if ($null -eq $crit) { $crit = $l }
    }

    # --- 新增场景 diff（含注册阶段）与步数变化 --------------------------------
    $newScen = New-Object System.Collections.ArrayList
    $addedReg = @(@($Cur.reg_scenarios) | Where-Object { @($Prev.reg_scenarios) -notcontains $_ })
    if ($addedReg.Count -gt 0) {
        [void]$newScen.Add([pscustomobject]@{ Lane = $null; Files = $addedReg; Where = "注册阶段" })
    }
    # persist 阶段（跨重启验证）的新增场景 diff：旧基线没有 persist_scenarios 字段
    # → 全部算新增，把 Δ 归因到 persist/重启而不是「环境波动」。
    $addedPersist = @(@($Cur.persist_scenarios) |
            Where-Object { @($Prev.persist_scenarios) -notcontains $_ })
    if ($addedPersist.Count -gt 0) {
        [void]$newScen.Add([pscustomobject]@{
                Lane = $null; Files = $addedPersist; Where = "persist 阶段（跨重启验证）"
            })
    }
    $stepChanged = New-Object System.Collections.ArrayList
    foreach ($l in @($Cur.lanes)) {
        $prevSet = @()
        $p = $null
        if ($prevLanes.ContainsKey([int]$l.id)) { $p = $prevLanes[[int]$l.id]; $prevSet = @($p.scenarios) }
        $added = @(@($l.scenarios) | Where-Object { $prevSet -notcontains $_ })
        if ($added.Count -gt 0) {
            [void]$newScen.Add([pscustomobject]@{ Lane = $l; Files = $added; Where = $null })
        }
        if ($null -ne $p -and [int]$l.steps -ne [int]$p.steps) {
            [void]$stepChanged.Add(("L{0} {1} steps {2}→{3}" -f $l.id, $l.desc, $p.steps, $l.steps))
        }
    }

    $regDelta = $regCur - $regPrev
    $laneDelta = $laneCur - $lanePrev
    [void]$out.Add(("Δ 构成              : 注册 {0}{1}s ｜ 各道并行墙钟 {2}{3}s" -f `
            $(if ($regDelta -ge 0) { "+" } else { "" }), $regDelta,
            $(if ($laneDelta -ge 0) { "+" } else { "" }), $laneDelta))

    if ($newScen.Count -eq 0) {
        [void]$out.Add("归因：未发现新增场景（各道场景清单与前次一致）。")
        if ($stepChanged.Count -gt 0) {
            [void]$out.Add(("  场景内容扩充：{0}" -f ($stepChanged -join '；')))
            [void]$out.Add("  → 拆分探索：把上述道内重复片段（repeat/等待密集段）继续拆到并行度有余量的道，或复核 sleep/timeout。")
        }
        else {
            [void]$out.Add("  各道步数亦无变化 → 疑为环境波动（服务器/网络/覆盖率开销），建议复跑确认。")
        }
        return [pscustomobject]@{ Lines = @($out); Regression = $true; CausedByNewScenario = $false; OverBudget = $overBudget }
    }

    # --- 归因到新增场景 -------------------------------------------------------
    foreach ($ns in $newScen) {
        $where = if ($ns.Where) { [string]$ns.Where }
        elseif ($null -eq $ns.Lane) { "注册阶段" }
        else { "L{0} {1}" -f $ns.Lane.id, $ns.Lane.desc }
        [void]$out.Add(("归因：新增场景 → {0} + {1}" -f $where, ($ns.Files -join '、')))
    }

    # 关键路径（当前最长道）及其增量
    $critId = [int]$crit.id
    $prevCritSec = if ($prevLanes.ContainsKey($critId)) { [int]$prevLanes[$critId].sec } else { [int]$crit.sec }
    $critDelta = [int]$crit.sec - $prevCritSec
    $critSign = if ($critDelta -ge 0) { "+" } else { "" }
    [void]$out.Add(("关键路径            : L{0} {1} {2}s（前次 {3}s，{4}{5}s）" -f `
            $critId, $crit.desc, $crit.sec, $prevCritSec, $critSign, $critDelta))

    $critNew = @($newScen | Where-Object { $null -ne $_.Lane -and [int]$_.Lane.id -eq $critId })
    $regCausedGrew = ($addedReg.Count -gt 0 -and $regDelta -gt 0)
    $critCausedGrew = ($critNew.Count -gt 0 -and $critDelta -gt 0)
    $causedByNew = $critCausedGrew -or $regCausedGrew

    if ($regCausedGrew -and -not $critCausedGrew) {
        # 注册阶段新增场景且注册变慢，关键路径道本身没变 → 归因注册
        [void]$out.Add("归因：注册阶段新增场景拉长了注册墙钟（各账号并行跑同一份 register.sce，单账号变长即整体变长）。")
        [void]$out.Add("  → 拆分探索：register.sce 无法按账号拆道；若新增步骤非必需，精简注册流程，或把耗时步骤移出注册（登录后再做）。")
        return [pscustomobject]@{ Lines = @($out); Regression = $true; CausedByNewScenario = $true; OverBudget = $overBudget }
    }

    if (-not $critCausedGrew) {
        if (@($stepChanged).Count -gt 0) {
            [void]$out.Add(("  关键路径无新增场景文件，但步数变化：{0}" -f (@($stepChanged) -join '；')))
        }
        [void]$out.Add("  关键路径的增长不含新增场景 → 增长主要来自其他因素（上方 Δ 构成 / 步数变化 / 环境）。")
        $laneNewOnly = @($newScen | Where-Object { $null -ne $_.Lane })
        if ($laneNewOnly.Count -gt 0) {
            [void]$out.Add("  新增场景落在非关键路径道，当前不改变墙钟；拆分它们不会缩短总耗时。")
        }
        return [pscustomobject]@{ Lines = @($out); Regression = $true; CausedByNewScenario = $false; OverBudget = $overBudget }
    }

    # --- 拆分探索（新增场景在关键路径上） -------------------------------------
    $costEst = $critDelta
    if ($costEst -le 0) { $costEst = 0 }
    $newCritSec = [int]$crit.sec - $costEst
    $others = @(@($Cur.lanes) | Where-Object { [int]$_.id -ne $critId })
    $othersMax = 0
    foreach ($o in $others) { if ([int]$o.sec -gt $othersMax) { $othersMax = [int]$o.sec } }
    $files = @($critNew | ForEach-Object { $_.Files }) -join '、'

    [void]$out.Add(("拆分探索（按关键路径增量 {0}s 近似新增场景成本，墙钟 = 注册 + 最长道）：" -f $costEst))
    if ($costEst -gt 0) {
        if ($others.Count -gt 0) {
            $target = $null
            foreach ($o in ($others | Sort-Object { [int]$_.sec })) { $target = $o; break }
            $newTotA = $regCur + [Math]::Max([Math]::Max($newCritSec, $othersMax), [int]$target.sec + $costEst)
            $saveA = $curExec - $newTotA
            [void]$out.Add(("  · 方案 A：把 {0} 从 L{1} 移到 L{2}（现 {3}s）→ 预计总耗时 {4}s（省 {5}s）" -f `
                    $files, $critId, $target.id, $target.sec, $newTotA, $saveA))
        }
        $newTotB = $regCur + [Math]::Max([Math]::Max($newCritSec, $othersMax), $costEst)
        $saveB = $curExec - $newTotB
        [void]$out.Add(("  · 方案 B：新增一条并行道（`$Lanes 加一项 + -Workers 调大）承接 {0} → 预计总耗时 {1}s（省 {2}s）" -f `
                $files, $newTotB, $saveB))
        if ($saveA -le 0 -and $saveB -le 0) {
            [void]$out.Add("  · 估算收益 ≤ 0：次长道已逼近关键路径，单纯移动不缩短墙钟——应拆分关键路径道内的既有片段。")
        }
    }
    else {
        [void]$out.Add("  关键路径较前次无增量（其他道/注册贡献了 Δ），先核对上方 Δ 构成再决定是否拆分。")
    }
    [void]$out.Add("  ⚠ 拆分前核对：新增场景是否依赖钉给某道的稀缺资源（集成测试指令 §8.4）、/t 目标（§8.5）。")

    return [pscustomobject]@{ Lines = @($out); Regression = $regressed; CausedByNewScenario = $causedByNew; OverBudget = $overBudget }
}

try {
    # --- 启动服务器（--config 指向集成测试配置） -----------------------------
    # 只回收上次运行残留的测试服务器（认端口不认进程名，不碰其他端口上的 jianghu）。
    # persist / reload 试验台端口同样先回收——残留实例会握着独立库文件句柄，删不掉库。
    Stop-OrphanTestServer -Port $Port
    Stop-OrphanTestServer -Port $SidePort
    Stop-OrphanTestServer -Port $ReloadPort
    # 上一轮的试验台结果先删：任务没跑起来时报告不得误读陈旧结果
    Remove-Item -LiteralPath $SideResultFile -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $ReloadResultFile -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1

    # --- 数据库（可选重建） ---------------------------------------------------
    # 位置必须在回收残留服务器**之后**：残留服务器仍握着库文件句柄时 Windows 下
    # 删不掉（SQLite 不共享删除权限），先回收再删才可靠。
    # SQLite 的「重建库」= 删库文件（连带 WAL/SHM 附属文件），服务端启动时
    # sqlite3_open_v2 自动重建、prepare_tables 自动建表播种，无需任何外部工具。
    if ($ResetDb) {
        Write-Host "-- 重建测试库 $DbPath"
        foreach ($f in @($DbPath, "$DbPath-wal", "$DbPath-shm")) {
            if (Test-Path $f) { Remove-Item -Force $f }
        }
        # 试验台独立库一并重建（跨重启持久化的就是它）
        Write-Host "-- 重建试验台库 $SideDbPath"
        foreach ($f in @($SideDbPath, "$SideDbPath-wal", "$SideDbPath-shm")) {
            if (Test-Path $f) { Remove-Item -Force $f }
        }
    }

    # 崩溃检测：服务器的未处理异常过滤器（CCreateDump）会往工作目录（仓库根）写
    # "[时间]dumpfile.dmp"。先把上一轮遗留的旧 dump 归档到仓库根 crash_dumps\
    # （在仓库里，看得见；也避免在仓库根无限堆积），再快照当前集合——收尾停服后
    # diff，新增的即本次崩溃证据。
    $oldDumps = @(Get-ChildItem -Path $RepoRoot -Filter "*.dmp" -File -ErrorAction SilentlyContinue)
    if ($oldDumps.Count -gt 0) {
        New-Item -ItemType Directory -Force -Path $OldDumpArchive | Out-Null
        foreach ($d in $oldDumps) {
            try {
                Move-Item -LiteralPath $d.FullName -Destination (Join-Path $OldDumpArchive $d.Name) -Force
            }
            catch {
                Write-Warning "归档旧 dump 失败（保留原地）：$($d.FullName) — $($_.Exception.Message)"
            }
        }
    }
    $dumpsBefore = @(Get-ChildItem -Path $RepoRoot -Filter "*.dmp" -File -ErrorAction SilentlyContinue |
            ForEach-Object { $_.FullName })

    # 服务端日志观测基线：记下各日志文件当前长度；停服后只扫此后的增量。
    # 文件名规则见 Logger.cpp（M-D.txt，月日不补零）——不猜文件名，直接快照
    # 目录里已有的全部 .txt，兼跨天/跨轮边界。
    $serverLogOffsets = @{}
    foreach ($f in @(Get-ChildItem -LiteralPath $ServerLogDir -Filter "*.txt" -File -ErrorAction SilentlyContinue)) {
        $serverLogOffsets[$f.Name] = $f.Length
    }

    $tServerStart = Get-Date
    Start-E2EServer

    if (-not (Wait-Port $Port $ServerReadyTimeoutSec)) { throw "服务器未在端口 $Port 就绪（超时 $ServerReadyTimeoutSec 秒）" }
    $tServerReadySec = [int]((Get-Date) - $tServerStart).TotalSeconds
    Write-Host "-- 服务器就绪"

    # --- persist 试验台：后台任务（文首第 10 条） ----------------------------
    # 独立端口/独立库的第二台服务器，按段表跑「段 → 重启 → 段」循环；在阶段 1
    # 之前启动，与注册和全部道**并行**——它的重启发生在各道跑到一半时，
    # 不占关键路径。任务自成一体（起服/注册/跑段/停服/重启/导出/结果 JSON），
    # 主流程只在道跑完后收结果。
    if ($PersistSegments.Count -gt 0) {
        # 上一轮残留的段覆盖率清掉：段数变化时避免误并入旧文件
        foreach ($f in @(Get-Item $SideCovGlob -ErrorAction SilentlyContinue)) {
            Remove-Item -Force $f.FullName
        }
        $SideLogRunId = "$LogRunId-pt"
        Write-Host "-- 启动 persist 试验台（端口 $SidePort，与各道并行，段间即重启）"
        $sideJob = Start-Job -Name "persist-testbed" -ArgumentList @(
            $RepoRoot, $ServerExe, $OccExe, $CliExe,
            [bool]$NoCoverage, [bool]$Dbg, $StepTimeout,
            $SideConfPath, $SideCliConf, $ScenarioDir, $WorkDir,
            $RunHex, $SidePort, $SideLogRunId,
            (ConvertTo-Json -InputObject @($PersistSegments) -Compress -Depth 5),
            $SideResultFile
        ) -ScriptBlock {
            param($Repo, $ServerExe, $OccExe, $CliExe, $NoCoverage, $Dbg, $StepTimeout,
                $SideConf, $SideCliConf, $ScenarioDir, $WorkDir,
                $RunHex, $SidePort, $SideLogRunId, $SegmentsJson, $ResultFile)

            $ErrorActionPreference = "Stop"
            # PS5.1 的 ConvertFrom-Json 把数组作为一个对象输出，管道外再套 @()
            # 会包成「单元素内含数组」的嵌套结构——直接赋值保扁平，仅对
            # 单对象 JSON（未来单段表）做标量兜底。
            $segments = $SegmentsJson | ConvertFrom-Json
            if ($segments -isnot [array]) { $segments = @($segments) }
            $result = [pscustomobject]@{
                status      = "fail"
                reason      = ""
                n           = $null
                expect_n    = $null
                persist_sec = 0
                restart_sec = 0
                segments    = @()
            }
            $serverHandle = $null
            $occHandle = $null

            # 参数拼接统一走引号（仓库路径可能含空格）
            function Join-Args([string[]]$a) {
                return (($a | ForEach-Object {
                        if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
                    }) -join ' ')
            }

            # 无窗口启动子进程并异步排空输出（与主流程 New-HiddenStartInfo 同构）
            function Start-HiddenProcess {
                param([string]$Exe, [string]$Arguments, [string]$OutFile, [string]$ErrFile)
                $psi = New-Object System.Diagnostics.ProcessStartInfo
                $psi.FileName = $Exe
                $psi.Arguments = $Arguments
                $psi.WorkingDirectory = $Repo
                $psi.UseShellExecute = $false
                $psi.CreateNoWindow = $true
                $psi.RedirectStandardOutput = $true
                $psi.RedirectStandardError = $true
                $psi.StandardOutputEncoding = [System.Text.Encoding]::UTF8
                $psi.StandardErrorEncoding = [System.Text.Encoding]::UTF8
                $p = New-Object System.Diagnostics.Process
                $p.StartInfo = $psi
                [void]$p.Start()
                return [pscustomobject]@{
                    Proc    = $p
                    StdOut  = $p.StandardOutput.ReadToEndAsync()
                    StdErr  = $p.StandardError.ReadToEndAsync()
                    OutFile = $OutFile
                    ErrFile = $ErrFile
                }
            }

            function Save-ProcessOutput($h, [int]$TimeoutMs = 20000) {
                try {
                    [void]$h.StdOut.Wait($TimeoutMs)
                    [void]$h.StdErr.Wait($TimeoutMs)
                    if ($h.StdOut.Result) { Set-Content -LiteralPath $h.OutFile -Value $h.StdOut.Result -Encoding UTF8 }
                    if ($h.StdErr.Result) { Set-Content -LiteralPath $h.ErrFile -Value $h.StdErr.Result -Encoding UTF8 }
                }
                catch { }
            }

            function Wait-SidePort([int]$Port, [int]$TimeoutSec) {
                $deadline = (Get-Date).AddSeconds($TimeoutSec)
                while ((Get-Date) -lt $deadline) {
                    if ((Test-NetConnection -ComputerName 127.0.0.1 -Port $Port -WarningAction SilentlyContinue).TcpTestSucceeded) { return $true }
                    Start-Sleep -Milliseconds 300
                }
                return $false
            }

            # 起试验台第 $SegNo 段实例：-NoCoverage 直接拉起；覆盖率模式经 OCC
            # 只导该段二进制 e2e_persist_seg<No>.cov（最终报告由主流程合并通行证出）
            function Start-SideServer([int]$SegNo) {
                if ($NoCoverage) {
                    $h = Start-HiddenProcess -Exe $ServerExe `
                        -Arguments (Join-Args @("--config", $SideConf, "--log-run-id", $SideLogRunId)) `
                        -OutFile (Join-Path $WorkDir ("side_server.seg{0}.stdout.log" -f $SegNo)) `
                        -ErrFile (Join-Path $WorkDir ("side_server.seg{0}.stderr.log" -f $SegNo))
                    $script:serverHandle = $h
                }
                else {
                    $cov = Join-Path $Repo ("coverage_history\e2e_persist_seg{0}.cov" -f $SegNo)
                    $occLog = Join-Path $WorkDir ("side_occ.seg{0}.log" -f $SegNo)
                    $h = Start-HiddenProcess -Exe $OccExe `
                        -Arguments (Join-Args @(
                                "--modules", $ServerExe,
                                "--sources", (Join-Path $Repo "src"),
                                "--optimized_build",
                                "--working_dir", $Repo,
                                "--export_type", "binary:$cov",
                                "--",
                                $ServerExe, "--config", $SideConf, "--log-run-id", $SideLogRunId
                            )) `
                        -OutFile $occLog -ErrFile "$occLog.err"
                    $script:occHandle = $h
                }
                if (-not (Wait-SidePort $SidePort 180)) {
                    throw "试验台第 $SegNo 段服务器未在端口 $SidePort 就绪"
                }
                Write-Output "试验台第 $SegNo 段服务器就绪"
            }

            # 优雅停服（认端口不认进程名，按 PID 关窗）→ 等 OCC 导出该段二进制
            function Stop-SideServer([int]$SegNo) {
                $conn = Get-NetTCPConnection -LocalPort $SidePort -State Listen -ErrorAction SilentlyContinue |
                    Select-Object -First 1
                if ($null -ne $conn) {
                    $p = Get-Process -Id $conn.OwningProcess -ErrorAction SilentlyContinue
                    if ($null -ne $p -and $p.ProcessName -eq 'jianghu') {
                        $serverPid = $p.Id
                        try { [void]$p.CloseMainWindow() } catch { }
                        $deadline = (Get-Date).AddSeconds(30)
                        while ((Get-Date) -lt $deadline -and (Get-Process -Id $serverPid -ErrorAction SilentlyContinue)) {
                            Start-Sleep -Milliseconds 500
                        }
                        Get-Process -Id $serverPid -ErrorAction SilentlyContinue | Stop-Process -Force
                    }
                }
                if ($script:serverHandle) {
                    Save-ProcessOutput $script:serverHandle
                    $script:serverHandle = $null
                }
                if ($script:occHandle) {
                    $occ = $script:occHandle.Proc
                    $deadline = (Get-Date).AddSeconds(180)
                    while ((Get-Date) -lt $deadline -and -not $occ.HasExited) { Start-Sleep -Milliseconds 500 }
                    if (-not $occ.HasExited) { try { $occ | Stop-Process -Force } catch { } }
                    Save-ProcessOutput $script:occHandle
                    $script:occHandle = $null
                    if (-not $NoCoverage) {
                        $cov = Join-Path $Repo ("coverage_history\e2e_persist_seg{0}.cov" -f $SegNo)
                        if (-not (Test-Path $cov)) { throw "试验台第 $SegNo 段覆盖率未导出（$cov）" }
                    }
                }
            }

            # 同步跑一段剧本：CLI 输出直接落盘（UTF-8），按剧本报告判定
            function Invoke-SideScenario {
                param([string]$ScriptRel, [string]$Vars, [string]$Report, [string]$Log)
                $cliArgs = @("--config", $SideCliConf,
                    "--script", (Join-Path $ScenarioDir $ScriptRel),
                    "--vars", $Vars,
                    "--step-timeout", "$StepTimeout",
                    "--report", $Report)
                if ($Dbg) { $cliArgs += "--dbg" }
                $h = Start-HiddenProcess -Exe $CliExe -Arguments (Join-Args $cliArgs) `
                    -OutFile $Log -ErrFile "$Log.err"
                $h.Proc.WaitForExit()
                Save-ProcessOutput $h
                $exit = $h.Proc.ExitCode

                $rep = @{}
                if (Test-Path $Report) {
                    foreach ($l in Get-Content -LiteralPath $Report -Encoding UTF8) {
                        if ($l -match '^([^=]+)=(.*)$') { $rep[$Matches[1]] = $Matches[2] }
                    }
                }
                $steps = 0
                if (Test-Path $Log) {
                    $raw = Get-Content -LiteralPath $Log -Raw -Encoding UTF8
                    $m = [regex]::Match($raw, '\[scenario\] PASS:\s*(\d+)\s*steps')
                    if ($m.Success) { $steps = [int]$m.Groups[1].Value }
                }
                $ok = ($exit -eq 0 -and $rep['status'] -eq 'pass')
                $reason = $rep['reason']
                if (-not $ok) {
                    if (-not $reason) { $reason = "CLI 退出码 $exit" }
                    if ($rep['line']) { $reason = "$reason（剧本行 $($rep['line'])）" }
                }
                return [pscustomobject]@{ Ok = $ok; Steps = $steps; Reason = $reason }
            }

            try {
                # 段 1 的服务器先起（注册也要打在试验台独立库上）
                Start-SideServer -SegNo 1

                # --- 注册各段账号（同 register.sce；名 = e2e<hex><Suffix>） -----
                foreach ($seg in $segments) {
                    $vars = Join-Path $WorkDir ("persist_{0}.vars" -f $seg.Id)
                    $name = "e2e$RunHex$($seg.Suffix)"
                    $varsText = "name=$($name)`npass=e2e1234`nemail=$($name)@example.com`ngender=男性`nattrs=21 21 21 21`n"
                    [System.IO.File]::WriteAllText($vars, $varsText,
                        (New-Object System.Text.UTF8Encoding($false)))
                    $r = Invoke-SideScenario -ScriptRel "register.sce" -Vars $vars `
                        -Report (Join-Path $WorkDir ("persist_{0}.register.report" -f $seg.Id)) `
                        -Log (Join-Path $WorkDir ("persist_{0}.register.log" -f $seg.Id))
                    if (-not $r.Ok) { throw "试验台账号注册失败（$($name)）：$($r.Reason)" }
                }
                Write-Output "试验台账号注册完成（$(($segments | ForEach-Object { $_.Id }) -join '、')）"

                # --- N 段循环：段间即一次重启 --------------------------------
                for ($i = 0; $i -lt $segments.Count; $i++) {
                    $seg = $segments[$i]
                    if ($i -gt 0) {
                        $t0 = Get-Date
                        Stop-SideServer -SegNo $i
                        Start-SideServer -SegNo ($i + 1)
                        $result.restart_sec += [int]((Get-Date) - $t0).TotalSeconds
                        Write-Output "试验台重启完成 → 第 $($i + 1) 段（$($seg.Id)）"
                    }

                    $vars = Join-Path $WorkDir ("persist_{0}.vars" -f $seg.Id)
                    if ($seg.Inject -eq "expect_n_plus1") {
                        # 跨段注入：读上一段 vars 捕获的 n，注入 expect_n = n+1
                        $prevVars = Join-Path $WorkDir ("persist_{0}.vars" -f $segments[$i - 1].Id)
                        $m = Select-String -LiteralPath $prevVars -Pattern '^n=(\d+)$' |
                            Select-Object -Last 1
                        if (-not $m) {
                            throw "上一段（$($segments[$i - 1].Id)）未捕获挑战计数 n（$prevVars），无法注入 expect_n"
                        }
                        $n = [int]$m.Matches[0].Groups[1].Value
                        $result.n = $n
                        $result.expect_n = $n + 1
                        Add-Content -LiteralPath $vars -Value "expect_n=$($n + 1)" -Encoding ASCII
                    }

                    $t0 = Get-Date
                    $r = Invoke-SideScenario -ScriptRel $seg.Script -Vars $vars `
                        -Report (Join-Path $WorkDir ("persist_{0}.report" -f $seg.Id)) `
                        -Log (Join-Path $WorkDir ("persist_{0}.log" -f $seg.Id))
                    $dur = [int]((Get-Date) - $t0).TotalSeconds
                    $result.persist_sec += $dur
                    $result.segments = @($result.segments) + [pscustomobject]@{
                        id     = $seg.Id
                        ok     = $r.Ok
                        steps  = $r.Steps
                        reason = $r.Reason
                        sec    = $dur
                    }
                    if (-not $r.Ok) { throw "段 $($seg.Id) 失败：$($r.Reason)" }
                    Write-Output "试验台段 $($seg.Id) PASS（${dur}s，$($r.Steps) steps）"
                }

                Stop-SideServer -SegNo $segments.Count
                $result.status = "pass"
            }
            catch {
                $result.reason = $_.Exception.Message
                try { Stop-SideServer -SegNo 0 } catch { }
            }
            finally {
                try {
                    $result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $ResultFile -Encoding UTF8
                }
                catch { }
            }
        }
    }

    # --- reload 试验台：后台任务（文首第 11 条） -----------------------------
    # 独立端口/独立库/私有脚本副本的第二台服务器，跑一条连续剧本链：改文件
    # （filecopy 写在片段里）→ 等重载结果（sys 广播）→ 断言行为。任务自成
    # 一体（重建副本/起服/跑链/日志断言/停服/导出/结果 JSON），主流程只在
    # 道跑完后收结果；与注册和全部道并行，不占关键路径。
    # 以 ScriptBlock 方式加载编排脚本：Start-Job 子进程的执行策略不受本脚本
    # 启动参数控制（-ExecutionPolicy Bypass 只作用于当前进程），读文件建块
    # 绕开「子进程禁跑脚本」；脚本内部靠注入的 $IntDir 定位路径。
    if (-not $Simple) {
        # 上一轮残留的 reload 覆盖率清掉：避免误并入旧文件
        foreach ($f in @(Get-Item $ReloadCovGlob -ErrorAction SilentlyContinue)) {
            Remove-Item -Force $f.FullName
        }
        Write-Host "-- 启动 reload 试验台（端口 $ReloadPort，与各道并行，链内改文件）"
        $testbedSb = [scriptblock]::Create(
            [System.IO.File]::ReadAllText($ReloadTestbedScript, [System.Text.Encoding]::UTF8))
        $reloadJob = Start-Job -Name "reload-testbed" -ScriptBlock $testbedSb -ArgumentList @(
            $ServerExe, $OccExe, $CliExe,
            [bool]$NoCoverage, [bool]$Dbg, $StepTimeout,
            $LogRunId, $ReloadResultFile, $IntDir
        )
    }

    # --- 阶段 1：一次性注册 N 个账号（并行） ---------------------------------
    $t0 = Get-Date
    $regItems = @()
    foreach ($lane in $ActiveLanes) {
        if ($lane.NoRegister) { continue }  # L9 巫师道：账号自举，无注册流程
        $regItems += @{
            Tag       = "L$($lane.Id)"
            Arguments = @("--config", $CliConf,
                "--script", (Join-Path $ScenarioDir "register.sce"),
                "--vars", $lane.VarsFile,
                "--step-timeout", "$StepTimeout",
                "--report", $lane.RegReport)
            LogFile   = Join-Path $WorkDir ("lane{0}.register.log" -f $lane.Id)
        }
    }
    if ($regItems.Count -eq 0) {
        # 本模式的账号全部自举（简单模式的巫师账号）：没有注册阶段
        Write-Host "-- 阶段 1：跳过注册（账号自举，无注册流程）"
        $regResults = @()
    }
    else {
        Write-Host "-- 阶段 1：并行注册 $($regItems.Count) 个账号"
        $regResults = Invoke-ParallelBatch -Items $regItems -Phase "reg"
        $regRan = $true

        $regBad = @($regResults | Where-Object { $_.ExitCode -ne 0 })
        foreach ($r in $regBad) {
            Write-Warning "账号注册失败：$($r.Tag)"
            Show-LaneTail $r 30
            Write-Host "        完整输出：$($r.LogFile)"
        }
        if ($regBad.Count -gt 0) { throw "$($regBad.Count) 个账号注册失败" }
        Write-Host "-- $($regItems.Count) 个账号注册完成"
    }
    foreach ($lane in $ActiveLanes) {
        if (-not (Test-Path $lane.VarsFile)) { throw "注册后缺少变量文件：$($lane.VarsFile)" }
    }
    $tRegSec = [int]((Get-Date) - $t0).TotalSeconds

    # 注册完成后回读各道 acct，向配对两道互注 peer_acct（对方数字账号 ID）。
    # /t 只收 ID：玩家=数字账号 ID（全服有效、跨房间），NPC/物品=英文 id
    # （当前环境）；显示名允许重名、不作目标——$peer 仅保留给文本断言，
    # 剧本定向对方一律 /t $peer_acct。
    foreach ($lane in $ActiveLanes) {
        if (-not $lane.PeerId) { continue }
        $peerLane = @($ActiveLanes | Where-Object { $_.Id -eq $lane.PeerId }) | Select-Object -First 1
        $peerAcct = $null
        if (Test-Path $peerLane.VarsFile) {
            $m = Select-String -Path $peerLane.VarsFile -Pattern '^acct=(.+)$' |
                Select-Object -Last 1
            if ($m) { $peerAcct = $m.Matches[0].Groups[1].Value.Trim() }
        }
        if (-not $peerAcct) {
            throw "注册后未取到 L$($peerLane.Id) 的 acct（$($peerLane.VarsFile)），无法注入 peer_acct"
        }
        Add-Content -Encoding ASCII $lane.VarsFile "peer_acct=$peerAcct"
    }

    # 观战道 spectator_acct 注入（同上）：L5 的 fight/pk 段跨房 tell 开战信号
    foreach ($lane in $ActiveLanes) {
        if (-not $lane.WitnessId) { continue }
        $witLane = @($ActiveLanes | Where-Object { $_.Id -eq $lane.WitnessId }) | Select-Object -First 1
        $witAcct = $null
        if (Test-Path $witLane.VarsFile) {
            $m = Select-String -Path $witLane.VarsFile -Pattern '^acct=(.+)$' |
                Select-Object -Last 1
            if ($m) { $witAcct = $m.Matches[0].Groups[1].Value.Trim() }
        }
        if (-not $witAcct) {
            throw "注册后未取到 L$($witLane.Id) 的 acct（$($witLane.VarsFile)），无法注入 spectator_acct"
        }
        Add-Content -Encoding ASCII $lane.VarsFile "spectator_acct=$witAcct"
    }

    # --- 阶段 2：并行开跑各道 -------------------------------------------------
    Write-Host "-- 阶段 2：并行开跑 $($ActiveLanes.Count) 条道"
    $t0 = Get-Date
    foreach ($lane in $ActiveLanes) {
        Write-Host ("     L{0}  {1,-14} {2}" -f $lane.Id, $lane.Desc, $lane.Script)
    }
    $laneItems = @()
    foreach ($lane in $ActiveLanes) {
        $laneArgs = @("--config", $CliConf,
            "--script", (Join-Path $ScenarioDir $lane.Script),
            "--vars", $lane.VarsFile,
            "--step-timeout", "$StepTimeout",
            "--report", $lane.ReportFile)
        if ($Dbg) { $laneArgs += "--dbg" }
        $laneItems += @{
            Tag       = "L$($lane.Id)"
            Arguments = $laneArgs
            LogFile   = $lane.LogFile
        }
    }
    $laneResults = Invoke-ParallelBatch -Items $laneItems -Phase "lane"
    $tLaneSec = [int]((Get-Date) - $t0).TotalSeconds

    # 汇总：以剧本报告（--report 的 status=）为准，CLI 退出码兜底
    foreach ($r in $laneResults) {
        $id = [int]($r.Tag -replace '^L', '')
        $lane = @($ActiveLanes | Where-Object { $_.Id -eq $id }) | Select-Object -First 1
        if ($null -eq $lane) { continue }
        $lane.Result = $r
        $lane.Steps = $r.Steps

        $rep = @{}
        if (Test-Path $lane.ReportFile) {
            foreach ($l in Get-Content -Encoding UTF8 $lane.ReportFile) {
                if ($l -match '^([^=]+)=(.*)$') { $rep[$Matches[1]] = $Matches[2] }
            }
        }

        if ($r.ExitCode -eq 0 -and $rep['status'] -eq 'pass') {
            $lane.Status = 'pass'
        }
        else {
            $lane.Status = 'fail'
            $reason = $rep['reason']
            if (-not $reason) { $reason = "CLI 退出码 $($r.ExitCode)" }
            if ($rep['line']) { $reason = "$reason（剧本行 $($rep['line'])）" }
            $lane.Reason = $reason
        }
    }

    $badLanes = @($ActiveLanes | Where-Object { $_.Status -ne 'pass' })
    foreach ($lane in $badLanes) {
        Write-Warning "L$($lane.Id) $($lane.Desc) 失败：$($lane.Reason)"
        Show-LaneTail $lane.Result 30
        Write-Host "        完整输出：$($lane.LogFile)"
    }

    if ($badLanes.Count -eq 0) {
        $status = "pass"
        Write-Host "-- $($ActiveLanes.Count) 条道全部通过"
    }
    else {
        throw "$($badLanes.Count)/$($ActiveLanes.Count) 条道失败"
    }

    # --- persist 试验台结果回收（任务与各道全程并行，此处汇合） --------------
    if ($sideJob) {
        Write-Host "-- 等待 persist 试验台任务收口..."
        $null = Wait-Job -Job $sideJob -Timeout 600
        $sideOut = (Receive-Job -Job $sideJob -ErrorAction SilentlyContinue | Out-String)
        if ($sideOut) {
            foreach ($l in ($sideOut -split "`r?`n")) {
                if ($l.Trim()) { Write-Host "   [试验台] $l" }
            }
        }
        $job = Get-Job -Id $sideJob.Id -ErrorAction SilentlyContinue
        $jobState = if ($job) { [string]$job.State } else { "Missing" }
        if ($jobState -eq 'Running') { Stop-Job -Job $sideJob -ErrorAction SilentlyContinue }
        Remove-Job -Job $sideJob -Force -ErrorAction SilentlyContinue
        $sideJob = $null
        if ($jobState -ne 'Completed') {
            throw "persist 试验台任务未正常结束（state=$jobState）"
        }
        if (-not (Test-Path $SideResultFile)) {
            throw "persist 试验台未产出结果文件（$SideResultFile）"
        }
        $sideResult = Get-Content -LiteralPath $SideResultFile -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($sideResult.status -ne 'pass') {
            throw "persist 试验台失败：$($sideResult.reason)"
        }
        Write-Host ("-- persist 试验台通过：n={0} expect_n={1}（段耗时 {2}s + 重启 {3}s，与各道并行、不占关键路径）" -f `
                $sideResult.n, $sideResult.expect_n, $sideResult.persist_sec, $sideResult.restart_sec)
    }

    # --- reload 试验台结果回收（任务与各道全程并行，此处汇合） --------------
    if ($reloadJob) {
        Write-Host "-- 等待 reload 试验台任务收口..."
        $null = Wait-Job -Job $reloadJob -Timeout 600
        $reloadOut = (Receive-Job -Job $reloadJob -ErrorAction SilentlyContinue | Out-String)
        if ($reloadOut) {
            foreach ($l in ($reloadOut -split "`r?`n")) {
                if ($l.Trim()) { Write-Host "   [reload] $l" }
            }
        }
        $job = Get-Job -Id $reloadJob.Id -ErrorAction SilentlyContinue
        $jobState = if ($job) { [string]$job.State } else { "Missing" }
        if ($jobState -eq 'Running') { Stop-Job -Job $reloadJob -ErrorAction SilentlyContinue }
        Remove-Job -Job $reloadJob -Force -ErrorAction SilentlyContinue
        $reloadJob = $null
        if ($jobState -ne 'Completed') {
            throw "reload 试验台任务未正常结束（state=$jobState）"
        }
        if (-not (Test-Path $ReloadResultFile)) {
            throw "reload 试验台未产出结果文件（$ReloadResultFile）"
        }
        $reloadResult = Get-Content -LiteralPath $ReloadResultFile -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($reloadResult.status -ne 'pass') {
            throw "reload 试验台失败：$($reloadResult.reason)"
        }
        Write-Host ("-- reload 试验台通过：链 {0}s / {1} steps + 服务端日志断言 {2} 项（与各道并行、不占关键路径）" -f `
                $reloadResult.chain_sec, $reloadResult.steps, @($reloadResult.log_checks).Count)
    }
}
catch {
    $status = "fail"
    $failReason = $_.Exception.Message
    Write-Warning "运行失败：$failReason"
}
finally {
    Stop-ServerGraceful -Port $Port

    # persist 试验台收尾：道失败提前跳出时任务可能还在跑——有界等待后强停，
    # 残留的试验台服务器按端口回收（只认 jianghu，与主套件同规）。
    if ($sideJob) {
        $null = Wait-Job -Job $sideJob -Timeout 120
        $sideOut = (Receive-Job -Job $sideJob -ErrorAction SilentlyContinue | Out-String)
        if ($sideOut) {
            foreach ($l in ($sideOut -split "`r?`n")) {
                if ($l.Trim()) { Write-Host "   [试验台] $l" }
            }
        }
        $job = Get-Job -Id $sideJob.Id -ErrorAction SilentlyContinue
        if ($job -and $job.State -eq 'Running') { Stop-Job -Job $sideJob -ErrorAction SilentlyContinue }
        Remove-Job -Job $sideJob -Force -ErrorAction SilentlyContinue
        $sideJob = $null
        try { Stop-OrphanTestServer -Port $SidePort }
        catch { Write-Warning "试验台端口 $SidePort 回收失败：$($_.Exception.Message)" }
    }
    # 道失败路径下也尽量带上试验台结果（报告要展示它）
    if ($null -eq $sideResult -and (Test-Path $SideResultFile)) {
        try {
            $sideResult = Get-Content -LiteralPath $SideResultFile -Raw -Encoding UTF8 | ConvertFrom-Json
        }
        catch { }
    }

    # reload 试验台收尾：道失败提前跳出时任务可能还在跑——有界等待后强停，
    # 残留的试验台服务器按端口回收（只认 jianghu，与主套件同规）。
    if ($reloadJob) {
        $null = Wait-Job -Job $reloadJob -Timeout 120
        $reloadOut = (Receive-Job -Job $reloadJob -ErrorAction SilentlyContinue | Out-String)
        if ($reloadOut) {
            foreach ($l in ($reloadOut -split "`r?`n")) {
                if ($l.Trim()) { Write-Host "   [reload] $l" }
            }
        }
        $job = Get-Job -Id $reloadJob.Id -ErrorAction SilentlyContinue
        if ($job -and $job.State -eq 'Running') { Stop-Job -Job $reloadJob -ErrorAction SilentlyContinue }
        Remove-Job -Job $reloadJob -Force -ErrorAction SilentlyContinue
        $reloadJob = $null
        try { Stop-OrphanTestServer -Port $ReloadPort }
        catch { Write-Warning "reload 试验台端口 $ReloadPort 回收失败：$($_.Exception.Message)" }
    }
    # 道失败路径下也尽量带上 reload 试验台结果（报告要展示它）
    if ($null -eq $reloadResult -and (Test-Path $ReloadResultFile)) {
        try {
            $reloadResult = Get-Content -LiteralPath $ReloadResultFile -Raw -Encoding UTF8 | ConvertFrom-Json
        }
        catch { }
    }

    # 崩溃检测：停服后 diff dump 快照。崩溃发生在关服路径（WM_DESTROY →
    # wait_for_all_worker_threads 期间工作线程仍在跑），剧本结果此时早已判定，
    # 不在这里检查就会「剧本全绿、服务器其实崩了」。
    # 新 dump **原地留在仓库根**，只打印完整路径——挪走（如挪到 %TEMP%）会让人
    # 找不到崩溃证据、误以为崩溃已修好。旧 dump 已在启动前归档到 crash_dumps\。
    $crashDumps = @()
    if ($null -ne $dumpsBefore) {
        $crashDumps = @(Get-ChildItem -Path $RepoRoot -Filter "*.dmp" -File -ErrorAction SilentlyContinue |
                Where-Object { $dumpsBefore -notcontains $_.FullName })
    }
    if ($crashDumps.Count -gt 0) {
        $status = "fail"
        $reason = "服务器崩溃（未处理异常），新增 dump: " +
        (($crashDumps | ForEach-Object { $_.FullName }) -join "、")
        if ($failReason) { $failReason = "$failReason；$reason" } else { $failReason = $reason }
        Write-Warning "服务器崩溃！新 dump 保留在仓库根（未移动）："
        foreach ($d in $crashDumps) {
            Write-Host ("       {0}（{1:N0} 字节，写于 {2}）" -f $d.FullName, $d.Length, $d.LastWriteTime)
        }
        Write-Host "       旧 dump 归档目录：$OldDumpArchive"
    }

    # 服务端日志观测（轨道 L）：剧本全绿但服务端报错同样是矛盾信号。停服后扫
    # 本次新增日志区间——未登记的 ERROR/FATAL 判 FAIL（与崩溃检测同级）；
    # 已登记的预期错误（故意负测等）静默；WARN 仅计数入报告。
    # 先点名本轮日志文件（按启动时传下的 run id 匹配，不猜日期——月日不补零的
    # 拼接交给通配）；试验台用 <runid>-pt 落同目录另一份，故后缀用通配一并点名。
    # 找不到说明 --log-run-id 没生效，如实说明而不是静默。
    $runLogFiles = @(Get-ChildItem -LiteralPath $ServerLogDir `
            -Filter "*-$LogRunId*" -File -ErrorAction SilentlyContinue)
    if ($runLogFiles.Count -gt 0) {
        $srvLogFileLine = "本轮服务端日志：" +
        (($runLogFiles | ForEach-Object { $_.FullName }) -join "、")
        Write-Host $srvLogFileLine
    }
    else {
        Write-Host "本轮服务端日志：未生成 *-$LogRunId*（查 $ServerLogDir）"
    }

    $srvLog = $null
    if ($null -ne $serverLogOffsets) {
        try { $srvLog = Get-ServerLogIssues -Offsets $serverLogOffsets -AllowFile $LogAllowFile }
        catch { Write-Warning "服务端日志扫描失败：$($_.Exception.Message)" }
    }
    if ($srvLog) {
        if ($srvLog.Unknown.Count -gt 0) {
            $status = "fail"
            $reason = "服务端日志出现未登记 ERROR/FATAL（$($srvLog.Unknown.Count) 条，非白名单）"
            if ($failReason) { $failReason = "$failReason；$reason" } else { $failReason = $reason }
            Write-Warning "服务端日志错误（未在 log-allowlist.txt 登记）："
            foreach ($u in ($srvLog.Unknown | Select-Object -First 10)) {
                Write-Host "       $u"
            }
            if ($srvLog.Unknown.Count -gt 10) {
                $fullLog = if ($runLogFiles.Count -gt 0) {
                    ($runLogFiles | ForEach-Object { $_.FullName }) -join "、"
                }
                else { $ServerLogDir }
                Write-Host "       （共 $($srvLog.Unknown.Count) 条，仅列前 10；完整日志见 $fullLog）"
            }
            Write-Host "       若确属预期（负测等），在 $LogAllowFile 登记其签名。"
        }
        $srvWarnCount = 0
        foreach ($k in @($srvLog.WarnByFn.Keys)) { $srvWarnCount += [int]$srvLog.WarnByFn[$k] }
        $srvAllowed = $srvLog.ErrTotal - $srvLog.Unknown.Count
        # 观测器自身要可见：全静默时至少报一行「扫描发生了、多少被白名单吸收」，
        # 否则无法区分「没有错误」与「没有扫描」。
        Write-Host ("服务端 ERROR    : {0} 条（白名单吸收 {1}，未登记 {2}）" -f `
                $srvLog.ErrTotal, $srvAllowed, $srvLog.Unknown.Count)
        if ($srvWarnCount -gt 0) {
            $warnSummary = (@($srvLog.WarnByFn.GetEnumerator() | Sort-Object Value -Descending |
                        ForEach-Object { "$($_.Key)×$($_.Value)" })) -join "、"
            Write-Host "服务端 WARN    : $srvWarnCount 条（仅报告不判失败：$warnSummary）"
        }
        # $lines 在 finally 之后才创建——summary 行先挂独立变量，后面再合并。
        $srvLogSummary = ("服务端日志：ERROR {0}（白名单 {1}，未登记 {2}），WARN {3}" -f `
                $srvLog.ErrTotal, $srvAllowed, $srvLog.Unknown.Count, $srvWarnCount)
    }

    # 整段墙钟收点（简单模式预算口径）：到关服与观测收尾为止；覆盖率导出在下方
    # 单列，与完整模式「导出不计入对比」的口径一致。
    if ($null -ne $tWallStart) { $tWallSec = [int]((Get-Date) - $tWallStart).TotalSeconds }

    # NoCoverage 模式下服务器 stdout/stderr 此前被异步排空后丢弃——落盘备查。
    if ($serverRun) {
        try {
            [void]$serverRun.StdOut.Wait(5000)
            [void]$serverRun.StdErr.Wait(5000)
            $stdoutTxt = $serverRun.StdOut.Result
            $stderrTxt = $serverRun.StdErr.Result
            $srvOutFile = Join-Path $WorkDir "server.stdout.log"
            $srvErrFile = Join-Path $WorkDir "server.stderr.log"
            if ($stdoutTxt) { Set-Content -LiteralPath $srvOutFile -Value $stdoutTxt -Encoding UTF8 }
            if ($stderrTxt) { Set-Content -LiteralPath $srvErrFile -Value $stderrTxt -Encoding UTF8 }
            if ($stdoutTxt -or $stderrTxt) {
                Write-Host "服务器输出    : $srvOutFile / $srvErrFile"
            }
        }
        catch { Write-Warning "服务器输出落盘失败：$($_.Exception.Message)" }
    }

    if (-not $NoCoverage -and $occRun) {
        # 主服实例导出二进制（最终报告由下方的合并通行证统一产出）
        $tExportSec = Wait-CoverageExport -OccRun $occRun `
            -LogName "e2e-sample-opencppcoverage.log"
    }

    # --- 覆盖率合并通行证（三路二进制 → 最终 html/cobertura/binary） ---------
    # OCC 启动时校验 --input_coverage 文件必须存在，故只能等主服与 persist
    # 试验台各段全部导出完毕后跑这一趟；child 用立即退出的 cmd（--sources 限定
    # 在 src 下，child 不贡献覆盖），合并结果 = 各输入的并集。
    if ($status -eq "pass" -and -not $NoCoverage) {
        $sideCovs = @(Get-Item $SideCovGlob -ErrorAction SilentlyContinue |
                Sort-Object Name | ForEach-Object { $_.FullName })
        $reloadCovs = @(Get-Item $ReloadCovGlob -ErrorAction SilentlyContinue |
                Sort-Object Name | ForEach-Object { $_.FullName })
        $mergeInputs = @($CovBinMain) + $sideCovs + $reloadCovs
        $missing = @($mergeInputs | Where-Object { -not (Test-Path $_) })
        if ($missing.Count -gt 0) {
            $status = "fail"
            $reason = "覆盖率输入缺失、无法合并：" + ($missing -join "、")
            if ($failReason) { $failReason = "$failReason；$reason" } else { $failReason = $reason }
            Write-Warning $reason
        }
        else {
            Write-Host "-- 覆盖率合并通行证（主服 + persist 试验台 $($sideCovs.Count) 段 + reload 试验台 $($reloadCovs.Count) 路）..."
            $tMerge = Get-Date
            $mergeArgs = @(
                "--sources", (Join-Path $RepoRoot "src"),
                "--working_dir", $RepoRoot,
                "--export_type", "html:$ReportDir",
                "--export_type", "cobertura:$ReportDir\coverage.xml",
                "--export_type", "binary:$CovBin"
            )
            foreach ($c in $mergeInputs) { $mergeArgs += @("--input_coverage", $c) }
            $mergeArgs += @("--", (Join-Path $env:SystemRoot "System32\cmd.exe"), "/c", "exit 0")
            $mergeRun = Start-HiddenProcess -Exe $OccExe -WorkingDirectory $RepoRoot -Arguments $mergeArgs
            $mp = $mergeRun.Process
            $deadline = (Get-Date).AddSeconds(180)
            while ((Get-Date) -lt $deadline -and -not $mp.HasExited) { Start-Sleep -Milliseconds 500 }
            if (-not $mp.HasExited) {
                Write-Warning "覆盖率合并通行证未退出，强制结束"
                try { $mp | Stop-Process -Force } catch { }
            }
            $mergeLog = Join-Path (Split-Path $CovBin) "e2e-sample-opencppcoverage-merge.log"
            try {
                @($mergeRun.StdOut.Result, $mergeRun.StdErr.Result) |
                    Where-Object { $_ } | Set-Content -Encoding UTF8 $mergeLog
            }
            catch { }
            $tExportSec = [int]$tExportSec + [int]((Get-Date) - $tMerge).TotalSeconds
            if (-not (Test-Path (Join-Path $ReportDir "coverage.xml"))) {
                $status = "fail"
                $reason = "覆盖率合并通行证未产出 coverage.xml（见 $mergeLog）"
                if ($failReason) { $failReason = "$failReason；$reason" } else { $failReason = $reason }
                Write-Warning $reason
            }
        }
    }
}

# ===========================================================================
# 报告
# ===========================================================================

$lines = New-Object System.Collections.ArrayList
$cov = $null

$covXml = Join-Path $ReportDir "coverage.xml"
# 失败时跳过覆盖率计算与展示：部分执行的覆盖率既无意义，又会刷屏淹没失败信息
if ($status -eq "pass" -and (-not $NoCoverage) -and (Test-Path $covXml)) {
    [xml]$xml = Get-Content $covXml -Encoding UTF8
    $totalLines = 0; $totalCovered = 0
    $zeroFiles = @{}
    $dirStat = @{}
    foreach ($pkg in $xml.coverage.packages.package) {
        foreach ($c in $pkg.classes.class) {
            $fn = [string]$c.filename
            if ($fn -notmatch '[\\/]src[\\/]') { continue }
            if (Test-CoverageSkippedFile $fn) { continue }
            $lineNodes = @($c.lines.line)
            $loc = $lineNodes.Count
            if ($loc -le 0) { continue }
            $covCount = @($lineNodes | Where-Object { [int]$_.hits -gt 0 }).Count
            $totalLines += $loc
            $totalCovered += $covCount

            $rel = ConvertTo-SrcRelative $fn
            if ($covCount -eq 0) { $zeroFiles[$rel] = $loc }

            $dir = "src"
            $m = [regex]::Match($rel, '^src\\([^\\]+)')
            if ($m.Success) { $dir = $m.Groups[1].Value }
            if (-not $dirStat.ContainsKey($dir)) { $dirStat[$dir] = @(0, 0) }
            $dirStat[$dir][0] += $loc
            $dirStat[$dir][1] += $covCount
        }
    }
    if ($totalLines -gt 0) {
        $cov = [pscustomobject]@{
            Percent    = [math]::Round(100.0 * $totalCovered / $totalLines, 1)
            Covered    = $totalCovered
            Total      = $totalLines
            Dirs       = $dirStat
            ZeroFiles  = $zeroFiles
        }
    }
}

$fnReport = $null
if ($cov) {
    Write-Host "-- 扫描源码，统计未命中函数..."
    $fnReport = Get-UnhitFunctionReport -CoberturaPath $covXml
}

# --- 控制台报告 -------------------------------------------------------------
Write-Host ""
Write-Host "================ 集成测试结果 ================"
if ($status -eq "pass") { Write-Host "状态        : PASS" } else { Write-Host "状态        : FAIL" }
if ($failReason) { Write-Host "失败原因    : $failReason" }
Write-Host "并行道数    : $($ActiveLanes.Count)（$(($ActiveLanes | ForEach-Object { $_.Name }) -join ' ')）"
foreach ($lane in $ActiveLanes) {
    $mark = if ($lane.Status -eq 'pass') { "PASS" } else { "FAIL" }
    $steps = if ($lane.Steps -gt 0) { "$($lane.Steps) steps" } else { "-" }
    Write-Host ("  L{0} {1,-14} {2}  ({3})" -f $lane.Id, $lane.Desc, $mark, $steps)
    if ($lane.Status -ne 'pass' -and $lane.Reason) {
        Write-Host ("      原因：{0}" -f $lane.Reason)
    }
}
if ($sideResult) {
    foreach ($seg in @($sideResult.segments)) {
        $mark = if ($seg.ok) { "PASS" } else { "FAIL" }
        $detail = @()
        if ($null -ne $sideResult.n -and $seg.id -eq 'pre') { $detail += "n=$($sideResult.n)" }
        if ($null -ne $sideResult.expect_n -and $seg.id -eq 'post') { $detail += "expect_n=$($sideResult.expect_n)" }
        if ($seg.steps -gt 0) { $detail += "$($seg.steps) steps" }
        $detail += "$($seg.sec)s"
        Write-Host ("  persist_{0} {1,-12} {2}  （试验台段，与各道并行 {3}）" -f `
                $seg.id, "跨重启", $mark, ($detail -join ' '))
        if (-not $seg.ok -and $seg.reason) {
            Write-Host ("      原因：{0}" -f $seg.reason)
        }
    }
    if ($sideResult.status -ne 'pass' -and $sideResult.reason) {
        Write-Host ("  persist 试验台    FAIL：{0}" -f $sideResult.reason)
    }
}
elseif ($PersistSegments.Count -gt 0) {
    Write-Host "  persist 试验台    未产出结果（排查见上方 [试验台] 日志与 test_tmp\e2e_lanes）"
}
if ($reloadResult) {
    $mark = if ($reloadResult.status -eq 'pass') { "PASS" } else { "FAIL" }
    $detail = @()
    if ($reloadResult.steps -gt 0) { $detail += "$($reloadResult.steps) steps" }
    $detail += "$($reloadResult.chain_sec)s"
    $detail += "日志断言 $(@($reloadResult.log_checks).Count) 项"
    Write-Host ("  reload 试验台    {0,-10} {1}  （与各道并行 {2}）" -f `
            "热重载链", $mark, ($detail -join ' '))
    if ($reloadResult.status -ne 'pass' -and $reloadResult.reason) {
        Write-Host ("      原因：{0}" -f $reloadResult.reason)
    }
}
elseif (-not $Simple) {
    Write-Host "  reload 试验台    未产出结果（排查见上方 [reload] 日志与 test_tmp\e2e_lanes）"
}
if ($status -ne "pass" -and -not $NoCoverage -and (Test-Path $ReportDir)) {
    Write-Host ""
    Write-Host "（集成测试失败：跳过覆盖率与未命中函数展示；报告仍在 $ReportDir）"
}

# --- 耗时监控：与前次同类通过运行对比、归因、记录（文首第 8 条） ----------------
# 只记录 PASS：失败运行的耗时被中断截断，不可比也不入历史。
$timingLines = @()
$timingReport = $null
if ($status -eq "pass" -and $null -ne $tRegSec -and $null -ne $tLaneSec) {
    $curRecord = [pscustomobject]@{
        time             = Get-Date -Format s
        status           = $status
        mode             = $(if ($Simple) { "simple" } else { "full" })
        workers          = $ActiveLanes.Count
        no_coverage      = [bool]$NoCoverage
        dbg              = [bool]$Dbg
        step_timeout     = $StepTimeout
        budget_sec       = $DurationBudgetSec
        budget_base      = $(if ($Simple) { "wall" } else { "exec" })
        build_sec        = $tBuildSec
        server_ready_sec = $tServerReadySec
        wall_sec         = $tWallSec
        reg_sec          = $tRegSec
        lane_sec         = $tLaneSec
        # persist / reload 试验台与各道并行，其耗时只作诊断展示、不进 exec 关键路径
        persist_sec      = $(if ($sideResult) { [int]$sideResult.persist_sec } else { $null })
        restart_sec      = $(if ($sideResult) { [int]$sideResult.restart_sec } else { $null })
        reload_sec       = $(if ($reloadResult) { [int]$reloadResult.chain_sec } else { $null })
        exec_sec         = $tRegSec + $tLaneSec
        export_sec       = $tExportSec
        reg_scenarios    = $(if ($regRan) { @(Get-LaneScenarioList "register.sce") } else { @() })
        persist_scenarios = @(foreach ($s in $PersistSegments) { Get-LaneScenarioList $s.Script })
        reload_scenarios = $(if ($reloadResult) { @(Get-LaneScenarioList "reload\reload_suite.sce") } else { @() })
        lanes            = @(foreach ($lane in $ActiveLanes) {
                [pscustomobject]@{
                    id        = [int]$lane.Id
                    desc      = $lane.Desc
                    script    = $lane.Script
                    sec       = if ($lane.Result) { [int]$lane.Result.DurationSec } else { 0 }
                    steps     = [int]$lane.Steps
                    scenarios = @(Get-LaneScenarioList $lane.Script)
                }
            })
    }

    # 同类基线：正序遍历历史（append 时间递增），反复覆盖后留下最近一条同配置 PASS 记录。
    # 必须在 Add-TimingRecord 之前读，否则会拿本次当基线跟自己比。
    # mode 区分简单/完整（旧记录无 mode 字段，都是完整模式运行）：两种模式的
    # 道构成与预算口径不同，互不作基线，否则 Δ 归因与拆分探索全无意义。
    $baseline = $null
    foreach ($h in @(Read-TimingHistory)) {
        if ($h.status -ne 'pass') { continue }
        if ([int]$h.workers -ne $curRecord.workers) { continue }
        if ([bool]$h.no_coverage -ne $curRecord.no_coverage) { continue }
        if ([bool]$h.dbg -ne $curRecord.dbg) { continue }
        if ([int]$h.step_timeout -ne $curRecord.step_timeout) { continue }
        $hMode = if ($h.mode) { [string]$h.mode } else { "full" }
        if ($hMode -ne [string]$curRecord.mode) { continue }
        $baseline = $h
    }

    $timingReport = Get-TimingReport -Cur $curRecord -Prev $baseline -ThresholdSec $DurationRegressionSec -BudgetSec $DurationBudgetSec
    $timingLines = @($timingReport.Lines)
    Write-Host ""
    Write-Host "================ 耗时监控 ================"
    foreach ($tl in $timingLines) { Write-Host $tl }
    if ($timingReport.OverBudget) {
        # 绝对预算告警：预算是护栏不是门禁（不改状态），但必须吵——滑动基线
        # 的教训是「每次 +N 都合法、总量单调爬升」，吵是唯一的收敛压力。
        $meas = Get-BudgetMeasure -Rec $curRecord
        Write-Host ""
        Write-Host ("!! {0}超绝对预算：预算 {1}s，实测 {2}s。" -f $meas.Label, $DurationBudgetSec, $meas.Sec)
        Write-Host "   预算是代码里的常数（run-integration-tests.ps1 默认值），调整须走 diff 评审；"
        Write-Host "   收敛手段见报告中的关键路径/吸收能力行与归因拆分建议。"
    }
    if ($timingReport.Regression) {
        Write-Host ""
        Write-Host ("（总耗时较前次同类运行增长超过参考阈值 {0}s——仅诊断提示，归因与拆分探索见上。）" -f $DurationRegressionSec)
    }
    Add-TimingRecord -Record $curRecord
}

$lines += $(if ($Simple) { "lua-sample E2E 简单模式：$status" } else { "lua-sample E2E：$status" })
if ($srvLogFileLine) { $lines += $srvLogFileLine }
if ($srvLogSummary) { $lines += $srvLogSummary }
if ($failReason) { $lines += "reason=$failReason" }
$lines += "workers=$($ActiveLanes.Count)"
foreach ($lane in $ActiveLanes) {
    $lines += ("L{0}={1} name={2} steps={3}{4}" -f $lane.Id, $lane.Status, $lane.Name,
        $lane.Steps, $(if ($lane.Reason) { " reason=$($lane.Reason)" } else { "" }))
}
if ($sideResult) {
    foreach ($seg in @($sideResult.segments)) {
        $extra = ""
        if ($null -ne $sideResult.n -and $seg.id -eq 'pre') { $extra += " n=$($sideResult.n)" }
        if ($null -ne $sideResult.expect_n -and $seg.id -eq 'post') { $extra += " expect_n=$($sideResult.expect_n)" }
        if ($seg.reason) { $extra += " reason=$($seg.reason)" }
        $lines += ("persist_{0}={1} steps={2} sec={3}{4}" -f $seg.id,
            $(if ($seg.ok) { "pass" } else { "fail" }), $seg.steps, $seg.sec, $extra)
    }
    $lines += ("persist_testbed={0} restart_sec={1}{2}" -f $sideResult.status,
        $sideResult.restart_sec, $(if ($sideResult.reason) { " reason=$($sideResult.reason)" } else { "" }))
}
elseif ($PersistSegments.Count -gt 0) {
    $lines += "persist_testbed=missing"
}
if ($reloadResult) {
    $extra = ""
    if ($reloadResult.reason) { $extra = " reason=$($reloadResult.reason)" }
    $lines += ("reload_testbed={0} steps={1} sec={2} log_checks={3}{4}" -f `
            $reloadResult.status, $reloadResult.steps, $reloadResult.chain_sec, `
        @($reloadResult.log_checks).Count, $extra)
}
elseif (-not $Simple) {
    $lines += "reload_testbed=missing"
}
$lines += "time=$(Get-Date -Format s)"
foreach ($tl in $timingLines) { $lines += $tl }

if ($cov) {
    Write-Host ""
    Write-Host "================ 覆盖率（src 行）=============="
    Write-Host ("行覆盖率    : {0}%（{1}/{2}）" -f $cov.Percent, $cov.Covered, $cov.Total)
    $lines += ""
    $lines += ("src 行覆盖率：{0}%（{1}/{2}）" -f $cov.Percent, $cov.Covered, $cov.Total)
    $lines += ""
    $lines += "--- 按二级目录 ---"
    foreach ($d in ($cov.Dirs.GetEnumerator() | Sort-Object { -$_.Value[0] })) {
        $sub = [math]::Round(100.0 * $d.Value[1] / [math]::Max(1, $d.Value[0]), 1)
        $row = ("  {0,-14} {1,6}/{2,-6} {3,5}%" -f $d.Key, $d.Value[1], $d.Value[0], $sub)
        Write-Host $row
        $lines += $row
    }
    $lines += ""
    $lines += ("0 命中文件（{0} 个，按 LOC 降序）——单骑 E2E 行走不到的引擎功能：" -f $cov.ZeroFiles.Count)
    foreach ($k in ($cov.ZeroFiles.GetEnumerator() | Sort-Object Value -Descending)) {
        $lines += ("  {0,5}  {1}" -f $k.Value, $k.Key)
    }
}

if ($fnReport) {
    $unhitList = @($fnReport.UnhitFunctions | Sort-Object File, Start)

    # --- N3 分段（§6 决策认领 vs 待推进）：与 e2e-unhit-claimed.txt 按清单原行匹配 ---
    $claimedSet = @{}
    if (Test-Path -LiteralPath $ClaimedFile) {
        foreach ($c in @(Get-Content -LiteralPath $ClaimedFile -Encoding UTF8 |
                    Where-Object { $_ -match '^src[\\\/].+?:\d+\s+\S' })) {
            $claimedSet[$c.Trim()] = $true
        }
    }
    $claimedList = @()
    $pendingList = @()
    foreach ($u in $unhitList) {
        $row = ("{0}:{1}  {2}" -f $u.File, $u.Start, $u.Name)
        if ($claimedSet.ContainsKey($row.Trim())) { $claimedList += $u } else { $pendingList += $u }
    }

    Write-Host ""
    Write-Host "================ 未命中函数 ==================="
    Write-Host ("近似统计    : 命中 {0}/{1}，未命中 {2} = §6 决策认领 {3} + 待推进 {4}（扫描 {5} 个文件）" -f `
            $fnReport.HitFunctions, $fnReport.TotalFunctions, $unhitList.Count, `
            $claimedList.Count, $pendingList.Count, $fnReport.ScannedFiles)
    # 「待推进」是唯一真实缺口口径：它就是下一批甄别/补测的全部对象
    if ($pendingList.Count -gt 0) {
        Write-Host "待推进缺口  : （真实缺口全名单）"
        foreach ($u in $pendingList) {
            Write-Host ("  {0}:{1}  {2}" -f $u.File, $u.Start, $u.Name)
        }
    }

    $listOut = New-Object System.Collections.ArrayList
    [void]$listOut.Add("# lua-sample E2E 未命中函数清单（近似，N3 分段）")
    [void]$listOut.Add("# 由 run-integration-tests.ps1 生成，口径与局限见 docs\design\integration-test.md §5.3")
    [void]$listOut.Add("# 命中 $($fnReport.HitFunctions)/$($fnReport.TotalFunctions)，未命中 $($unhitList.Count) = 决策认领 $($claimedList.Count) + 待推进 $($pendingList.Count)，扫描 $($fnReport.ScannedFiles) 个文件")
    [void]$listOut.Add("")
    [void]$listOut.Add("=== 待推进（真实缺口：下批要做的全部对象）===")
    foreach ($u in $pendingList) {
        [void]$listOut.Add(("{0}:{1}  {2}" -f $u.File, $u.Start, $u.Name))
    }
    [void]$listOut.Add("")
    [void]$listOut.Add("=== §6 决策认领（已按 design §6 域级裁定认领；新增归档时追加 coverage_history\e2e-unhit-claimed.txt）===")
    foreach ($u in $claimedList) {
        [void]$listOut.Add(("{0}:{1}  {2}" -f $u.File, $u.Start, $u.Name))
    }
    New-Item -ItemType Directory -Force -Path (Split-Path $UnhitFile) | Out-Null
    $listOut | Set-Content -Encoding UTF8 $UnhitFile
    Write-Host ("完整清单    : {0}" -f $UnhitFile)

    if ($unhitList.Count -le $UnhitListLimit) {
        foreach ($u in $unhitList) {
            Write-Host ("  {0}:{1}  {2}" -f $u.File, $u.Start, $u.Name)
        }
    }
    else {
        Write-Host ("（清单共 {0} 条，超过控制台上限 {1}，仅报数字；完整清单见上）" -f $unhitList.Count, $UnhitListLimit)
    }

    $lines += ""
    $lines += ("未命中函数：{0}/{1}（未命中 {2} = 决策认领 {3} + 待推进 {4}，扫描 {5} 个文件）" -f `
            $fnReport.HitFunctions, $fnReport.TotalFunctions, $unhitList.Count, `
            $claimedList.Count, $pendingList.Count, $fnReport.ScannedFiles)
    $lines += ("  完整清单：$UnhitFile")
}
else {
    if (-not $NoCoverage) {
        if ($status -eq "pass") { $lines += "（未生成覆盖率报告）" }
        else { $lines += "（测试失败，跳过覆盖率展示）" }
    }
}

New-Item -ItemType Directory -Force -Path (Split-Path $SummaryFile) | Out-Null
$lines | Set-Content -Encoding UTF8 $SummaryFile
Write-Host ""
Write-Host "== 汇总写入 $SummaryFile =="

if ($status -ne "pass") { exit 1 }
