# User 层

APRV（`VENT_MD_APRV`）按设定时间维持高压并短暂释放到低压，`GetVentAprvSettings()` 支持本地/主机来源，`vt aprv` 或 `vt mode 11` / `vt run 1` 启动。设置新增上升时间、压力/流量触发及独立压力/容量后备参数，移除独立压力限值和未实现的 `releaseCycleOffPercent`，使用全局报警压力高限。

| APRV 项目 | 当前约定 |
|---|---|
| 周期 | 高压 200..30000 ms、低压 192..30000 ms，上升时间不得超过高压时间；启动调零后先保持一个低压时间，再进入高压。每次实际切换重新计时，支持 tick 回绕 |
| 压力释放 | 高压保持 timeHighMs 后切低压，低压参考直接设为 pressureLowCmh2o；现有呼气阀释放/捕获控制执行降压。到 timeLowMs 即重新升压，不等待捕获完成 |
| 自主呼吸 | 高低水平均保持对应压力，无额外 PSV 或同步切换窗口。高压升压及稳定保护后以高压为触发基线；低压沿用稳定捕获和最小呼气保护。有效吸气只重置窒息计时，不改变级间时钟。高低压切换时重建触发基线，避免释放过程误触发 |
| 后备 | apneaSwitch 选择 OFF、压力或容量。使用全局 apneaTimeAlarm；高低压定时交替不算患者努力。超时报警后，在低压阶段满足最小呼气保护时启动独立频率/Ti/压力或容量后备；患者触发后从高压恢复 APRV。后备频率无需高于释放频率。OFF 关闭后备，沿用 SIMV 语义 |
| 压力保护 | 高压须大于等于零的低压且严格小于报警高限；实时高限降低裁剪后续目标，达到高限提前释放。异常设置拒绝应用 |
| 监测与验证 | 监测结果按完整计划周期发布，平台内自主努力不单独产生计划序号。主机测试覆盖定时释放、触发不改时序、压力限制、两种后备及恢复、非法参数和协议映射；尚未进行实机测试肺验证 |

APRV MCM 模式值为 `0x0B`，参数映射：`0x06` 氧浓度、`0x0A/0x0B` 高/低压力、`0x1B/0x1C` 高/低时间（秒转毫秒）、`0x1A` 上升时间、`0x11/0x12` 触发阈值、`0x24/0x25` 触发选择；后备沿用 `0x0C/0x0F/0x16/0x19` 及后备开关。接口由现有 VentTask/协议任务调用，不用于 ISR。


BAPAP（DuoLevel，`VENT_MD_BAPAP`）按高/低压力及持续时间交替通气，`GetVentBapapSettings()` 支持本地/主机来源，控制台 `vt bapap` 或 `vt mode 10` / `vt run 1` 启动。设置移除独立 `pressureLimitCmh2o`，统一读取报警压力高限，并增加压力/容量窒息后备参数。

| DuoLevel 项目 | 当前约定 |
|---|---|
| 时间输入 | 以 `timeHighMs`、`timeLowMs` 为直接控制量；界面如采用频率、I:E 等替代输入，应先换算为高低持续时间。高压 200..30000 ms，低压 192..30000 ms |
| 低压吸气同步窗 | 成人低压末 5000 ms，儿童/婴幼儿末 1500 ms；小于该长度时覆盖整个低压阶段，仍遵守最小呼气和稳定捕获保护。窗内触发立即转高压，窗末未触发则定时转高压 |
| 低压 PSV | 窗外患者触发按 `pressureLowCmh2o+pressureSupportCmh2o` 支持，支持压 0 表示无附加支持；按 `cycleOffPercent` 流量切换，并受 `maxInspiratoryTimeMs` 限制。PSV 不重置低压时钟，必要时缩短以保留高压前的最小呼气时间 |
| 高压同步窗 | 高压最后 1/4 内，检测到新的正向流量峰后下降到峰值×cycleOffPercent，经 3 个采样确认可提前转低压；此前的机器充气峰不用于此窗口，避免陈旧流量下降误触发。未触发则保持完整 timeHighMs |
| 高压自主呼吸 | 压力控制与既有超压呼气阀调节维持高水平，不对每次自主吸气重新升压。升压及稳定保护后，吸气触发按高压基线检测，仅更新窒息计时，不重启高压计时 |
| 级间计时 | 从实际高→低切换开始计完整低压时间，包含提前呼气切换；高压时钟从实际低→高切换开始，支持 32 位 tick 回绕 |
| 窒息后备 | 使用全局 `apneaTimeAlarm`，定时高低压交替不作为患者努力。按 `apneaSwitch` 选择压力/容量后备，使用独立目标、频率及 Ti；进入低压并满足最小呼气保护后可启动后备，患者吸气触发恢复同步控制。OFF 沿用 SIMV 的关闭后备语义 |
| 压力与验证 | 高压必须大于低压且小于报警高限，支持目标也须低于报警高限。实时下调报警高限裁剪新目标；达到高限提前切换低压。主机测试覆盖窗口、级间计时、PSV、阀门泄压和后备，尚未做测试肺验证 |
| 监测边界 | 沿用现有计划周期发布监测结果；高压平台内的自主努力用于同步/窒息检测，不单独产生新的呼吸计划序号 |

BAPAP MCM 参数映射：`0x06` 氧浓度，`0x0A/0x0B` 高/低压力，`0x1B/0x1C` 高/低时间（秒换算毫秒），`0x09` 支持压力，`0x11/0x12` 吸气触发阈值，`0x13` 呼气切换比例，`0x18` PSV 最大吸气时间，`0x1A` 上升时间，`0x24/0x25` 触发选择；后备沿用 `0x0C/0x0F/0x16/0x19` 和后备开关。模式下发值为 `0x07`（本地枚举值为 10）。


VS（`VENT_MD_VS`）采用患者触发、流量切换的容量支持；`GetVentVsSettings()` 支持本地/主机来源，控制台 `vt vs` 或 `vt mode 9` / `vt run 1` 启动。`cycleOffPercent` 直接决定流量切换阈值，校验为严格大于 0、小于 100 的有限值；默认 25%，支持主机 `0x13` 下发。

| VS 项目 | 当前约定 |
|---|---|
| 首个实际呼吸 | 压力支持为 PEEP+10 cmH₂O，受报警高限−5 裁剪；启动等待期间的预取计划和后备呼吸不计入试验次数 |
| 患者节律 | 压力/流量触发启动，流量下降到峰值×cycleOffPercent 后连续确认 3 个采样切换呼气；普通 VS 不设置定时吸气，保留最小吸气 200 ms、最大吸气 2000 ms 和最小呼气 192 ms 的保护，升压时间来自 riseTimeMs |
| 有效肺力学 | 使用 Ceff=VTI/ΔP、Reff=60×ΔP/Qpeak 作为包含呼吸努力的有效估计；无闭塞、无肌肉压测量时无法独立辨识真实静态 C/R，这些估计不作为静态肺力学测量结果 |
| 容量反馈 | 有效流量切换周期完成后，下一驱动压=当前驱动压×目标 VT/VTI；前三个 VS 周期步长不超过 10 cmH₂O，第 4 个起不超过 3，过量送气会减压；异常、限压、最大吸气超时及重复反馈不推动学习 |
| 实时压力限制 | 普通 VS 计划与压力控制器均执行报警高限−5，达到边界提前呼气，实时降限优先于调压步长；压力受限时不能保证达到目标 VT |
| 窒息时钟 | 无有效患者触发达到 apneaAlarmTimeMs（1000..60000 ms）后进入后备；MCM 窒息报警秒数同步转换为该字段。患者触发恢复 VS 并重置时钟，后备按独立频率持续运行，不更新 VS 学习 |
| 后备选择 | apneaSwitch 支持压力/容量后备，分别使用独立压力/VT、频率和 Ti，沿用后备控制器限压；OFF 时只产生窒息报警并等待患者触发，不送后备呼吸 |
| 重置与验证 | 停止/重启、设置或模式变更、调零后重新试验；已覆盖首压、步长、异常反馈、可调流量切换、后备及恢复、限压和协议映射；尚未进行实机测试肺验证 |


PRVC（`VENT_MD_PRVC`）复用现有 Scheduler、Phase、Flow/Pressure Controller 和 Monitor 链路，`GetVentPrvcSettings()` 按本地/主机来源返回设置；MCM 同步 FiO₂、PEEP、频率、吸气时间、目标潮气量和触发。控制台 `vt prvc` 启动，`vt trigger` 支持 PRVC。PRVC-SIMV（`VENT_MD_PRVC_SIMV`）也复用此调压逻辑，仅对常规指令呼吸学习。

PRVC-SIMV 使用 `GetVentPrvcSimvSettings()` 选择本地/主机参数，控制台 `vt prvcsimv`（或 `vt mode 8` / `vt run 1`）启动，`vt trigger` 支持此模式。首个常规指令呼吸为容量试验，后续为 PRVC 压控容量保证；窗外支持及窒息后备不会推进 PRVC 周期数或更新其压力反馈。

| PRVC-SIMV 项目 | 当前约定 |
|---|---|
| 指令频率 | 以 `60000/SIMVRateBpm` 为独立周期；窗外支持呼吸不重置指令时钟 |
| 同步窗 | 成人 5000 ms，小儿/婴幼儿 1500 ms，取与计划呼气时间的较小值；窗口位于下一次指令截止时间之前，起点包含在内 |
| 窗内触发 | 输送一次 PRVC 指令呼吸，并从这次实际指令吸气重新计时，不再在旧截止时间重复送气 |
| 无触发 | 达到窗口末端按时输送指令呼吸，仍保留最小呼气保护 |
| 窗外触发 | `supportPressureCmh2o` 为相对 PEEP 支持压，0 表示无附加支持；默认升压 200 ms，按 `cycleOffPercent` 切换，最长吸气 2000 ms，必要时缩短以保留下一指令前的呼气时间 |
| 窒息后备 | 按 `apneaSwitch` 选择关闭、压力或容量后备；后备采用独立目标和频率，不混入 PRVC 学习，复用现有 SIMV 后备恢复逻辑 |
| 主机参数 | 使用 SIMV 频率 `0x15`，不使用 AC 频率 `0x14`；同步 FiO₂、PEEP、VT、Ti、触发、支持压、呼气切换和窒息后备参数 |
| 压力限制 | 常规指令执行 PRVC 的报警高限减 5 限制；PSV 和窒息后备沿用既有 SIMV 控制器限压 |
| 验证 | 覆盖三种患者窗口、全呼气窗口、边界前/边界处触发、定时送气、时钟回绕、压力/容量后备和恢复、支持/后备反馈隔离；尚未进行实机测试肺验证 |

| PRVC 项目 | 当前约定 |
|---|---|
| 首周期 | 实际第一个吸气为容量试验，吸气时间末 10% 暂停；启动建立 PEEP 的预取计划不计入周期 |
| 肺力学估算 | 有效试验结果计算 C=VTI/(Pplat−PEEP)，R=60×(Ppeak−Pplat)/试验期实测峰流量（恒流近似）；C 单位 mL/cmH₂O，R 单位 cmH₂O/(L/s)。使用单室被动 RC 模型估算 ΔP=VT/[C×(1−exp(−Ti/(RC)))]，指数由固定 8 次迭代近似，未复用 HMI 的阻力公式 |
| 后续控制 | 固定吸气时间压力控制，默认升压段 200 ms（不超过 Ti）；按上个有效周期 VTI 对驱动压作比例修正，过量则减压、欠量则增压 |
| 步长 | 试验周期计为第 1 周期，第 2/3 周期最多调整 10 cmH₂O，第 4 周期起最多 3；`maximumPressureStepCmh2o` 必须为正有限值，可进一步收紧，两方向均限步长；默认 10 |
| 初始基准与无效反馈 | 初始调压基准为 PEEP+min(10, 设置步长)，受压力上限裁剪；试验失败后仍进入压力控制并保持此基准。无效、未完成、提前切换或控制器限幅周期不参与调压；同序号只消费一次，拒绝旧配置/旧序号 |
| 压力边界 | 容量试验和后续压控均受报警高限−5 cmH₂O 限制；实时达到边界提前呼气，下一计划及执行器同时裁剪。安全降压优先于步长；上限不能高于 100，减 5 后必须大于 PEEP；压力受限时不承诺达到目标 VT |
| 重置 | 停止/重启、模式切换、患者或 PRVC 设置变化、流量调零后重新试验；重复应用同一组设置及仅改变报警限值不会重启学习 |
| 验证范围 | 主机测试覆盖 RC 估算、顺应性/阻力变化后的收敛、首周期完整链路、步长切换、去重、重启、异常反馈、实时限压、MCM 参数和触发；尚未完成测试肺实机验证 |


吸气压力未达到报警按完整周期判断：检测 P 为控制器吸气相最高患者压力，P目标为本周期计划 `inspiratoryPressureCmh2o`（总目标压力），随周期结果保存。连续 3 个完整周期同时满足 `P < P目标-3.0 && P < P目标*0.6666` 后触发，否则清零计数并恢复；周期中保持，每个结果序号只计一次。无效压力、非正/无效目标、丢失周期序号打断连续计数，停止和监测复位时清除，计数在 3 饱和。

容量限制报警在每个完整呼吸周期结束后将本周期 `stBreathResult.vtiMl`（吸入潮气量，mL）与呼出潮气量报警上限 `limitSettings->tidalVolumeHigh` 比较，严格大于时触发，否则清除，所有呼吸类型均适用，周期中保持。上限在周期结束时保存为 `tidalVolumeLimitMl`，每个结果序号只处理一次；无效 VTI、缺失上限、停止或监测复位时清除。

压力限制报警在每个完整呼吸周期结束后更新一次：只有本周期 `breathType == BREATH_TYPE_MANDATORY_VOLUME`（flowcontrol）且吸气相最高患者压力 > `limitSettings->pressureHigh - 5.5` 才触发，否则清除，周期中保持。Monitor Engine 将周期结束时的 Pmax 保存至 `stBreathResult.pressureLimitCmh2o`，与本周期类型和吸气峰压一起发布，后续改限值或换计划不改变旧结果。吸气压力无效、Pmax 无效或缺失时不触发；流量无效不阻止有效压力判断。停止和监测复位时清除。

管路断开检测在完整周期采集机器吸入量 Vi（正向机器流量积分）、患者吸入量 Vpi（有符号吸气积分）、患者呼出量 Vpe（呼气负向流量积分），均为 mL；患者峰压、正向峰流量和最后一个吸气流量均按控制器吸气相采集。检测专用 `R = Ppeak * 60 / Qpeak`，`C = Vpi / max(Ppeak - min((Qend / 60) * R, 3), 0.001)`，不替换 HMI 阻力/顺应性。连续两个完整周期满足 `Vi > 50 && R < 10 && Vpe < 0.125*Vi && Vpi-Vi < 0.5*Vi`，首次 C>450、第二次 C>200 后触发；不满足或数据无效则重新开始。零/非正峰流量或 R 不参与除法判定。另一独立路径使用本周期限幅前的泄漏系数，连续 5 周期 >50 触发，<=50、无效或结果序号不连续则重计。报警保持至实时满足患者压力 >5、吸气压力 >15、机器流量 <0.3*Qpipe，立即恢复并清除检测历史；停止、调零或报警初始化时清除。

管路泄漏报警使用完整呼吸周期内 `MONITOR_LEAK_FLOW` 的最大值 `stBreathResult.peakLeakLpm`（L/min），与 `BREATH_RESULT_VALID_MINUTE_LEAK` 共用估计有效性。Monitor Engine 在吸气和呼气采集最大值，每次新呼吸清零，完整周期结束发布；`techPhysPipelineLeakDetect` 每个结果序号只处理一次：峰值 >5 计数加一，<3 清零，3..5（含边界）保持，计数 >4 报警，否则恢复。计数在 5 饱和以防溢出；无效估计保持计数和报警，停止、监测复位或报警初始化时清除。

吸气支路堵塞在 monitor 实时采集 `deltaP = INSP_REAL_PRS - PAT_REAL_PRS`（cmH₂O）、`Q = INSP_REAL_FLOW`（L/min）及当前吸气压力对应的 `Qpipe`。`app/ventlogic/pipeflowtable.c/.h` 固化成人/儿童和新生儿各 6 个标定点，按患者类型选择，表内线性插值，表外端点限幅。`techPhysInspBranchBlockageDetect` 在通气期间连续满足 `deltaP > 10 && Q <= 1.5 * deltaP` 1 秒触发；触发后连续满足 `Q > max(0.5 * Qpipe, 15)` 1 秒恢复。条件中断或数据无效时重新计时，停止时清除。monitor 将参数和有效位一起发布，AlarmTask 快照读取。

管路堵塞参数按控制器吸气相采集：患者压力峰值及末个吸气压力减吸气开始压力、患者流量绝对峰值（L/min）、无死区有符号流量积分（mL）。完整周期发布后，`techPhysPipelineBlockageDetect` 使用既有吸气阻力 R 判断 `(deltaPpeak >= 5 && R > 600 && Qpeak/deltaPpeak < 0.2)` 或 `(deltaPend >= 3 && abs(V)/deltaPend < 1.5 && Qpeak/deltaPend < 0.2)`；一个周期满足即触发，下一个不满足即清除，周期中保持状态。无效吸气数据不触发，停止或监测复位时清除。

MCM 呼吸频率 `0x10/0x11/0x12`（总频率/机控/自主）由 `60000 / 完整周期毫秒数` 计算，上传时按非负数四舍五入取整（14.49→14、14.5→15），保留现有 0..255 限幅和 scale=0 编码。

呼气峰值流速复用 Monitor Engine 的呼气峰值累计：呼气监测阶段取 `PAT_REAL_FLOW` 负向最大幅值，沿用 0.5 L/min 死区，以非负 L/min 表示。完整周期结束发布至 `stBreathResult.peakExpiratoryFlowLpm` 和 `MONITOR_HMI_PEAK_EXP_FLOW`，下一周期内保持；沿用 `volumeInvalid` 周期有效性检查，无效周期置零且不置 `BREATH_RESULT_VALID_PEAK_EXP_FLOW`，有效零流量可上传零。停止、调零及无有效计划时清零。MCM 通过 `0x0F` 逐呼吸上报，无符号 16 位、数值 ×10、scale=1，沿用监测数据队列重试。

吸入/呼出分钟通气量使用最新完整呼吸周期的潮气量（mL）×60÷实测周期时长（ms），单位 L/min，每完成一次呼吸更新。吸入值为 `stBreathResult.minuteInspiratoryLpm` / `MONITOR_HMI_MV_INSP`，呼出值复用 `minuteTotalLpm` / `MONITOR_HMI_MV_TOTAL`；这两个值是逐呼吸折算值，不是过去 60 秒滚动累计。周期时长为零、流量积分无效或结果非有限值时不置对应 `BREATH_RESULT_VALID_MVI/MVE`；有效零流量仍上报零。停止、调零和无有效计划时随监测状态清零。CommTask 通过 MCM 监测参数 `0x08`（吸入）、`0x09`（呼出）逐呼吸上传，使用既有无符号 16 位、数值 ×10、scale=1 编码及队列重试机制。

PAC 平台压取计划吸气结束前 100 ms 内的实测患者压力均值，有限的残余流量不再阻止采样；只在控制器吸气相采样，排除呼气降压数据，压力或流量非有限值时跳过该点。完整呼吸结束时更新平台压结果、有效位及 HMI，沿用逐呼吸上传链路。提前结束且没有末段有效样本时仍为无效。此值为 PAC 吸气末压力估计，非零流量时不等同于闭塞测得的静态平台压；VAC、PSV/ST 保留原近零流量采样条件。

当前 User 层包含板级外设驱动、RTOS 启动、任务骨架和基于 SEGGER RTT 的日志/控制台系统。

| 路径 | 职责 |
|---|---|
| `main.c` | 初始化日志和控制台、注册 worker tasks 并启动调度器 |
| `app/calibration/` | 上电只读加载 EEPROM 中五类既有校准记录，校验记录头与 CRC-16，并提供有效性查询和运行期只读访问 |
| `app/system/taskmanager.*` | 通过 `WorkerTasksRegister()` 创建 defaultTask、VentTask、SensorTask、SysTask、AlarmTask、CommTask；全部任务使用绝对周期 `DelayUntil`，SensorTask 每 3 ms 采集传感器并处理风机反馈，VentTask 每 6 ms 执行通气控制链 |
| `app/protocol/` | 保留 MCM 帧与接收缓存；CommTask 每 10 ms 收发，VentTask 应用参数和启停，20 ms 上发波形；旧机型功能以条件编译保留 |
| `bsp/uart/uart.*` | USART0 PA9 TX / PA10 RX，115200、8N1；中断收发，2048 字节 RX 缓存；公开 API 仅供任务调用 |
| `app/databus/` | 维护控制数据数组；SensorTask 保存当前及前一周期原始数据，VentTask 基于最新原始数据完成滤波和校准转换 |
| `app/ventalgo/` | 实现吸气压力、吸气流量、公共 Release/PEEP 和 FiO₂ 控制器；各控制器只生成统一 `stActuatorRequest`，不直接写 BSP |
| `app/ventlogic/` | Scheduler 为 PAC/VAC/PSV/PSV-ST/P-SIMV/V-SIMV/PRVC/PRVC-SIMV/VS/BAPAP 生成逐次 `stBreathPlan`，Phase Controller 执行计划，Trigger Engine 检测患者触发，Cycle Engine 完成 PSV 流量切换，Apnea Engine 调度 PSV 窒息后备和 PSV-ST 周期机控呼吸，Monitor Engine 发布逐次 `stBreathResult`，Actuator Controller 统一仲裁并写入 BSP |
| `app/ventlogic/pipeflowtable.*` | 固化成人/儿童与新生儿管路压力-流量表，为 Monitor Engine 的吸气支路堵塞恢复阈值提供 Qpipe |
| `app/physalarm/` | `physalarmmanager.*` 在 AlarmTask 注册、调度和发布 0xAD 生理报警；`physalarmvent.*` 实现检测；`alarmbits.h` 定义旧协议六组枚举及 union 位域 |
| `app/techalarm/` | `techalarmmanager.*` 独立注册、调度、发布技术报警并生成 0xAB 快照；`techphys.*`、`techdevice.*`、`techpower.*`、`techcomm.*`、`techcal.*` 分别承接 SubId 0..4 检测 |
| `bsp/adc/adc.*` | 使用 ADC1 规则组扫描、连续转换和 DMA1 循环模式持续采集 14 路板级模拟量 |
| `bsp/blower_vcm/blower_vcm.*` | 使用 UART4（板级 VCM UART5，PC12/PD2）和 DMA0 异步发送双控制帧、循环接收反馈；控制变化时立即发送并每 10 ms 保活重发，提供连接超时与通信统计 |
| `bsp/bspdebug.*` | 注册 `bsp` RTT 调试命令；支持 ADC、阀门、风机控制，以及 `bsp blower stats` 通信诊断 |
| `bsp/dvalve/dvalve.*` | 以枚举选择氧气阀、泄压阀或呼气阀，提供统一的 20 kHz、0～100% PWM 占空比控制接口 |
| `bsp/eeprom/m24512r.*` | 使用 PD10/PD11 软件 I2C 访问 M24512-R，提供跨页写入和任意字节读取；上电校准加载流程仅调用读取接口，不改写 EEPROM |
| `bsp/sf06sdk/sfm3119.*` | 管理两只 SFM3119；空气通道使用 PB6/PB7、200 kHz 硬件 I2C0 和 DMA 异步接收，氧气通道使用 PA12/PA11 优化模拟 I2C；启动后等待首个测量结果，每 2 ms 更新流量，每 100 个周期（200 ms）更新温度与状态，并缓存产品 ID 与序列号 |
| `bsp/valve/valve.*` | 初始化 4 路零点阀控制输出和状态反馈输入，并提供按阀门枚举访问的接口 |
| `module/log/` | RTT 日志、ringbuffer 输出队列和控制台命令；`vt` 支持 PAC/VAC/PSV/PSV-ST 启停、当前模式压力/流量触发设置、Cycle/Apnea 状态及呼吸结果/瞬态诊断 |
| `module/rtos/rtos.*` | 项目层任务、调度、固定周期 `DelayUntil`、tick 和临界区接口 |
| `module/rtos/portrtos.*` | FreeRTOS 原生接口绑定 |
| `tools/controller/` | 轻量控制算法；当前提供带输出限幅和积分抗饱和的固定周期浮点 PID |
| `tools/ringbuffer/` | 日志输出使用的轻量级字节环形缓冲区 |
| `develop/` | VS Code Device Tool 的 CMake 构建、烧录、复位与 RTT 工具；`test_flow_pause.py` 验证暂停控制，`test_monitor_leak.py` 验证泄漏估计，`test_vti_compensation.py` 验证逐呼吸补偿，`test_vti_rtt.py` 通过 Device Tool RTT 记录启动收敛 |

`vt volume <peep> <ml> [pause_pct [ti_ms rate]]` 通过 RTT 设置 VAC 参数；暂停百分比为 0..99，省略时保留当前值，上电默认 0；可在暂停值后成对指定吸气毫秒数及每分钟频率，命令校验后应用，拒绝挤占最短呼气时间的组合。`vt trigger` 按当前 PAC/VAC/PSV/PSV-ST 模式修改触发，VAC 不再落入 PAC 分支。`vt volume 15 500 0` 设置无暂停，供气覆盖完整吸气时间，目标流量按有效供气时间计算，`vt run 1` 启动；`vt status` 波形的 `volume_pause` 标记暂停阶段，`pause_settled` 标记已切入稳定段 PI，`leak_lpm` 记录患者侧泄漏估计。`develop/test_vac_matrix.py` 经 Device Tool RTT 入口完成模拟肺九组测试，保存原始日志、波形及逐呼吸振幅、泄漏目标误差统计；过零次数仅作辅助诊断。
| `FreeRTOSConfig.h` | FreeRTOS 工程配置 |

项目代码只能通过 `rtos.h` 使用任务、调度、tick 和临界区能力；FreeRTOS 原生 API 仅允许出现在 `portrtos.c`。日志统一使用 `LOG_I`、`LOG_W`、`LOG_E` 等宏，不能直接使用标准库输出函数。

`module/rtos/portrtos.c` 的毫秒转 tick 在乘法前提升到 64 位，避免默认 `pdMS_TO_TICKS` 在 1000 Hz 下运行约 71 分 35 秒后溢出，使最高优先级 SysTask 忙循环并饿死其他任务。`develop/test_rtos_timing.py` 使用真实端口代码验证该边界、长时间运行及当前 1000 Hz 配置下的 32 位 tick 回绕。

呼气 RELEASE 在有限的患者压力连续 60 ms 不高于 PEEP + |PEEP|×5% 后进入 CAPTURE，压力跌穿 PEEP 也满足释放完成条件，避免因无法回到狭窄压力窗口而永久屏蔽患者触发。CAPTURE 保留稳定判定及 750 ms 超时进入 PEEP 的路径。Trigger Engine 在呼气阶段即可准备基线，实际触发仍须呼气捕获完成、3 点连续确认和最短呼气保护通过。压力准备屏蔽高于 PEEP + 0.5 cmH₂O 的残余高压；流量准备不要求压力稳定或流量过零。低压本身不作为患者触发事件。`develop/test_trigger.py` 使用真实呼气控制器、PID 和触发引擎验证低压恢复及重复触发，传感器和相位接口使用主机桩；不代表已完成实机气路验证。

PAC 高压力控制：每次吸气按总目标压力 `PEEP + DeltaPressure` 调度内环 Kp，使用 `0.015 × clamp((20 / max(target, 20))², 0.25, 1)`，仅 PAC 生效；目标 20 cmH₂O 及以下维持原增益。PAC 管路流量模型输入上限为缩放后的 50（实际 100 L/min），PEEP≤5 时压降补偿仍受 `DeltaPressure × 0.3` 限制，绝对上限为 20 cmH₂O。高于 20 cmH₂O 的 PAC 平台期直接跟随已滤波流量计算的补偿下降值，避免叠加滤波延迟；继续禁止平台补偿回升。现有低流量稳压衔接、压力目标限幅和呼吸定时保持。`vt set <peep> <delta> [ti_ms rate rise_ms]` 可明确设置 PAC 测试定时，`vt status` 输出 `VT_PAC_SETTINGS`；测试脚本及验收口径见 `develop/README.md`。

PAC 较高 PEEP 的平台平坦度补偿：仅总目标 >20 cmH₂O 时，按 `w=clamp((PEEP-5)/5,0,1)` 平滑启用附加补偿，保留 PEEP≤5 的原计算。流量压降模型乘 `1+0.1w`，相对上限为 `(DeltaPressure+PEEP×w)×0.3`，仍受 20 cmH₂O 绝对上限约束；PEEP 10、ΔP 25 时从旧 7.5 上限放宽至 10.5，补足填充前段压降。平台名义前馈继续只能下降；其 6 ms 差分斜率按 0.05 增益平滑，患者压力首次到达目标减 0.2 后锁存启用 200 ms 下降提前量，最大扣减 `4w` cmH₂O，有效前馈限于零至名义值，用于补偿风机减速滞后。提前量基于流量模型，未向压力 PID 加入微分。新呼吸清除斜率与锁存，低流量稳压衔接仍走原路径；RTT `flowcomp` 记录实际施加的前馈。原 PID 参数、增益调度、升压限速、定时与泄压策略保留，PSV/ST 不启用此补偿。

PAC 高 PEEP 填充尾段减速缓冲：已捕获平台并启用流量提前量的 PAC，只有在近端流量降至 40 L/min 以下时距计划吸气结束仍至少 200 ms，才锁存本次吸气的缓冲资格；临近呼气的降流保留原控制。有资格时，近端流量从 40 降至 20 L/min 的过程中，线性启用基于实际风机减速度的临时转速补偿。每 6 ms 对实际转速差分作 0.1 增益滤波，按 300 ms 提前量计算，只补偿下降速度，最多增加 40 RPS；压力从目标升至目标+0.5 cmH₂O 时补偿平滑减至零。补偿在 PI 积分之外施加，转速稳定后自然消退，不把临时缓冲写入积分。进入 HOLD 时清空速度历史；无效转速禁用缓冲，恢复有效时重新建基线。PEEP≤5、未捕获平台、PSV/ST 不启用；原 PID 增益、流量前馈和低流量衔接条件保留。RTT 的 `innerEffort` 包含实际施加的减速缓冲，以保持执行器诊断可核对。

PAC 平台期在近端流量降至 20 L/min 及以下、患者压力进入目标 ±2 cmH₂O 后，本次吸气锁存低增益稳压：外环 Kp=0.25，内环 Kp=0.001、Ki=0.01。剩余流量前馈清零并通过 `pidTrackOutput` 转入内环输出；衔接实际风机转速，相对上一指令最多修正 40，反馈为零、非有限或超出 800 时回退上一指令。该处理用于减少填充结束时继续降速及随后的反复修正，不锁死风机；压力闭环和平台超压泄压继续工作。PAC 平台泄压借用呼气下降的趋势预测思路：每 6 ms 对患者压力差分斜率做 0.2 增益滤波，以 60 ms 预测量修正泄压误差，预测修正限于 ±1 cmH₂O；下降时提前收阀，上升时提前排气，原 0.8 cmH₂O 死区和最大 5% 开度保留。进入平台时重置预测历史，PSV/ST 保留原泄压算法；吸气压力目标和定时均保持原计划。内环积分在叠加前馈后的执行器饱和时撤回同方向增量。下一次吸气恢复原增益并清除锁存和积分；PSV/ST 不进入此分支。`develop/test_flow_pause.py` 验证捕获边界、稳压方向、预测收阀、持续超压泄压、模式隔离与重置；`develop/test_pac_rtt.py` 通过 Device Tool 显式设置并回读 PAC 参数，分别统计吸气后半段反向流量、末尾 600 ms 稳定段和末尾 240 ms，并核对压力参考切换时长。Monitor 的 Ti 含负流量确认延迟，不能作为控制切换时长。

2026-09-08 默认 PAC 无额外漏口模拟肺对比：PEEP 5、Delta-P 25 cmH₂O、Ti 1350 ms、20/min、氧浓度 21%、触发关闭。均排除前两次启动呼吸，使用参考下降前留出 12 ms 的末段 240 ms；按相同的随后十次呼吸比较，原版平均流量峰峰值 11.49 L/min，两次独立启动的最终固件分别为 5.622 和 5.168 L/min，降低 51.1% 和 55.0%。最终两组各分析 20 次呼吸，全组平均峰峰值分别为 4.930 和 4.561 L/min，最大单次为 9.08 和 7.50 L/min；末段压力范围分别为 29.33..30.69 和 29.15..30.57 cmH₂O。结果表明末段摆动减小，未完全消除；此窗口不包含此前填充结束的全部流量下降过程。数据为设备自身传感器测量，未覆盖其他顺应性、漏气或参数组合。原始数据位于 `build/pac_baseline/`、`build/pac_final_run1/`、`build/pac_final_run2/`，统一窗口统计为 `build/pac_comparison.json`；均经 Device Tool 编译/烧录/RTT 入口，测试结束已收到停止确认。

2026-09-10 当前默认 PAC 模拟肺回归：PEEP 5、Delta-P 25 cmH₂O、Ti 2000 ms、20/min、氧浓度 21%、BTPS、触发关闭。对照当前原固件与修改后两次独立启动，排除前两次启动呼吸，统一比较随后 12 次完整呼吸。吸气后半段逐呼吸最低 patflow 均值从 −5.478 降为 −3.460 / −3.158 L/min，负向过冲幅值降低 36.8% / 42.3%；从近端流量首次降至 20 L/min 起统计，最低值相同，未把过冲移出半段窗口。末尾 600 ms 平均峰峰值从 5.363 降至 4.375 / 4.212 L/min，降低 18.4% / 21.5%；最后 240 ms 分别为 3.408、3.807、3.268 L/min，此更短窗口的改善不一致。两次修改版末尾 240 ms 平均压力为 30.942 / 30.959 cmH₂O，对照为 30.559，存在约 +0.4 cmH₂O 的额外稳态偏差；半段压力范围分别为 30.04..31.57 / 30.14..31.65 cmH₂O。第二次修改版共 20 次有效呼吸，全组最低流量均值 −3.342 L/min、末尾 600 ms 峰峰值均值 4.161 L/min，控制参考切换时长均为 2004 ms。此结果只证明当前模拟肺默认工况的回冲及较长稳定窗口振幅减小，未消除摆动、未验证其他顺应性及泄漏工况。

原始记录位于 `build/pac_20260910_baseline/`、`build/pac_20260910_balanced/`、`build/pac_20260910_final/`，统一比较与波形图为 `build/pac_20260910_comparison.json` / `.png`，重算脚本为 `build/pac_20260910_compare.py`。`balanced` 初次报告的 timing 检查错误地使用了含负流量确认延迟的 Monitor Ti，随后根据真实控制逻辑改用压力参考切换时刻并重算；最终独立运行通过此检查。Device Tool 编译、烧录校验和 RTT 完成，控制器主机回归通过，最终收到 `stop status=1`。

VAC 支持按 `gVentVacSettings.triggerType` 选择关闭、压力或流量吸气触发，阈值分别来自 `pressureTriggerCmh2o` 和 `flowTriggerLpm`。患者触发仍生成容量控制呼吸，保留容量补偿、吸气时间、暂停和压力限制；无患者触发时按计划的呼气时长定时启动下一次呼吸。触发入口复用 PAC 的呼气捕获、基线准备、连续确认及最短呼气保护。`develop/test_trigger.py` 覆盖 VAC 两类触发和关闭；`develop/test_vti_compensation.py` 用真实 Scheduler/Phase/Trigger 验证容量计划、最短呼气屏蔽和下一次定时呼吸。

`triggerengine.c` 将压力和流量的基线更新及候选判定拆为独立函数，公共入口只负责选择路径、生命周期与连续确认。更换计划或触发类型后重新准备，切换类型不继承旧候选。

流量路径保留有符号基线，按 `PAT_REAL_FLOW - flowBaselineLpm >= flowTriggerLpm` 判定。8 点准备后，未形成候选且 `PAT_REAL_FLOW < 0` 时，以 0.5 增益逐样本跟随负流量；下降流量同样更新。流量回到非负且没有候选时，将残余负基线归零，不向上学习正向吸气斜坡。先判定再更新，候选连续确认期间冻结基线，因此基线约 -20、当前流量 -9、阈值 10 时也能触发，无须等呼完。波形 `proximalFlowX2` 是 `PAT_REAL_FLOW / 2`。主机回归覆盖负流量触发、线性回零、0.2/0.4/0.6 s 时间常数的指数呼气回零，以及 300 ms 正向流量斜坡；真实 Scheduler/Phase 回归覆盖负流量触发的最短呼气保护及 VAC 容量计划。

压力路径要求连续至少 8 点落在跨度 0.1 cmH₂O 内的稳定窗口，偏离窗口则重新计数；准备完成后，稳定、无候选且不高于 PEEP + 0.5 的样本继续以 0.1 增益构建基线。比较 `pressureBaselineCmh2o - PAT_REAL_PRS >= |pressureTriggerCmh2o|`，比较基线仍以设定 PEEP 为上限；候选冻结学习并清除稳定计数。短暂反弹不立即重建，持续稳定的新压力可以重建。回归覆盖稳定基线向上/向下更新、300 ms 从 2.5 降到 0 的吸气努力及连续确认。

上述稳定窗口与增益是当前软件参数，不保证区分所有被动呼气与吸气努力：负流量快速自然回升可能超过阈值，缓慢吸气也可能被移动基线吸收；压力变化若仍落在稳定窗口内也会被学习。实机气路、噪声、漏气和其他呼气时间常数尚未验证，合成回归不是截图原始数据回放。

当前 GD32F470 板载 HXTAL 为 8 MHz，系统使用 `240M_PLL_8M_HXTAL` 配置；该配置决定 RTOS tick 和 APB 外设（包括 VCM UART 230400）的实际时基。

PEEP 报警以当前呼吸计划的 `peepCmh2o` 为 refpeep。在新吸气阶段，`TECH_ALARM_PEEP_HIGH` 按上一周期 `MONITOR_DYN_PEEP > refpeep + 5` 触发，`TECH_ALARM_PEEP_LOW` 按 `< refpeep - 3` 触发，每个吸气计划只判断一次。AlarmTask 每 10 ms 检查恢复：动态 PEEP 严格低于高限或严格高于低限持续至少 200 ms 后解除对应报警；等于阈值会中断恢复计时。恢复不使用实时患者压力。无上一完整周期时不触发，停止通气或进入零点补偿阶段时清除 PEEP 报警和计时。检测器仅由 AlarmTask 调用，通过 RTOS 临界区读取 VentTask 的计划、阶段和监测快照，不用于 ISR。

泄漏估计按 `涡轮 → inpFlowSensor → 呼气阀排气支路接点 → midFlowSensor → lung` 的气路定义；呼出气体反向经过 midFlowSensor 后由呼气阀排出。仅 midFlowSensor 下游泄漏计入患者侧补偿，`inpFlow - midFlow` 包含呼气阀正常排气，不能直接作为暂停的近端流量目标。

Monitor 按实际吸气计划的 sequence 切分泄漏累计窗口，必须观察到呼气阶段才结算上一周期，不依赖 midFlow 是否出现负值。原有 VTi/VTe 的负流量呼气确认逻辑保留，与泄漏窗口独立。停止、零点补偿阶段、无有效计划或近端流量重新调零时清除旧估计及未完成周期；吸气中失效后等待下一次吸气边界，第一完整周期结束前使用零补偿。任一样本流量/压力非有限或压力不大于零，整个估计窗口无效；该窗口结束时清除旧 K、回退零补偿，不用删减后的样本发布估计。

`MONITOR_LEAK_BALANCE_COEFFICIENT` 保存有效窗口计算的带符号原始比值；`MONITOR_LEAK_COEFFICIENT` 是限幅到 0..50 的非负 K；`MONITOR_LEAK_VALID` 表示上一窗口通过完整性与数值检查。`MONITOR_LEAK_FLOW` 为 K 乘当前患者压力平方根后的统一补偿量，暂停控制与平台压零流量判定共同使用。独立上限 `MONITOR_PATIENT_LEAK_FLOW_MAX_LPM` 暂保留原值 120 L/min，尚非重新验证的台架限值。无效窗口的原始比值置零，应结合有效标志读取。

上述有效标志不证明肺内储气量首尾相同，也不检测传感器陈旧数据；固定 6 ms、周期首尾储气量近似一致和漏口特性近似恒定仍是估计前提。净储气变化及传感器偏差仍可能被识别为泄漏。暂停期不根据瞬时 midFlow 重新学习 K，避免将供气尾流变成补偿目标。

VAC 容量外环沿用 `MONITOR_TIDA_VOL_INSP` 的近端 VTI 定义（含吸气暂停阶段，未扣患者侧泄漏）。Phase Controller 在加载下一次吸气计划前调用 `monitorEngineBreathComplete()`，结算上一完整周期并将 VTI 送入 Scheduler；Monitor 的 sequence 边界处理保留为补充入口，通过完成标志避免重复发布。`monitorEngineProcess()` 仍仅按顺序调用处理函数。只有同一配置代次、当前计划序号且未消费过的反馈可以参与下一计划，旧配置结果不会重新激活补偿。

首次有效 VTI 直接初始化 `filteredVtiMl`，之后执行 `filteredVtiMl += 0.5 * (vtiMl - filteredVtiMl)`；已施加的等效容量补偿使用相同 EMA，误差扣除尚未反映在均值中的补偿，避免重复追补。死区为目标的 0.5%，首次增益 0.8，后续 1.0。`breathscheduler.h` 中 `BREATH_VOLUME_FLOW_COMPENSATION_ENABLE=0` 默认关闭初始损失流量补偿和逐呼吸流量补偿；设为 1 恢复旧路径（容量步长 ±25%、累计 ±30%）。新路径固定流量为用户目标除以原始供气时长，按容量误差换算供气时间调整，每呼吸最多 ±20 ms，累计不超过原始供气时长 ±30%。增加供气时间时等量扣减呼气时间，保留至少 192 ms 呼气；暂停时长不变，正常定时周期保持不变。毫秒取整后的实际补偿回写外环以防积分饱和。`volumeCorrectionMl` 表示时间调整的等效容量，`deliveryTargetMl` 仍用于压力前馈。所有调整只作用于下一呼吸，首次有效反馈恢复压力上限快照，避免启动调零误清除首周期补偿。

无有效完整周期、非有限/非正 VTI、未确认实际呼气、非正常时间切换、供气段压力/流量/风机上限或控制器失败时，外环不更新 EMA 和补偿。`BREATH_RESULT_VOLUME_LIMITED` 标记被限幅的周期；非有限采样清除结果中的 `BREATH_RESULT_VALID_VTI`。停机、重新启动、模式或 VAC 设置变化、重新调零会清理外环；压力上限改变时下一计划清理旧补偿。容量修正仅作用于下一周期供气段，暂停目标仍由患者侧泄漏估计决定。

`vt status` 的 `VT_VOLUME_FEEDBACK` 行输出当前计划序号、用户目标、平滑 VTI、补偿量和内部供气目标；体积字段均为 mL 的百分之一。`test_vti_compensation.py` 分别编译新时间路径和旧流量路径，验证 EMA、实际周期结算时序、异常反馈、限幅、配置重置，并验证 300/500/800 mL 固定流量模型的双向时间补偿；`test_vti_rtt.py` 支持 `--ti-ms` 和 `--rate`，明确关闭触发并核对 VAC 命令回执及结果模式，保存原始波形与验收 JSON，要求末尾连续五次有效且未限幅的完整呼吸满足 ±(10 mL + 目标×5%)；软件模型通过不等于已验证实机肺容量或气路稳定性。

VAC 供气流量闭环与 VTI 使用同一个患者侧近端流量 `PAT_REAL_FLOW`，不使用入口流量作为反馈。`vt status` 波形追加 `flow_ref_lpm`、`flow_measurement_lpm`、`flow_effort` 和 `flow_blower_ff`（均放大 100 倍），用于直接检查参考跟随、PID 输出和前馈。叠加前馈后的风机指令发生限幅时，控制器撤回同方向积分增量，避免执行器限幅造成的积分饱和。

RTT 启动回归（500 mL、PEEP 5、Ti 2000 ms、15 次/分、无暂停）保存在 `build/vti_rtt/comparison.json` 及对应目录的原始日志/波形。原版本第 7 次结果才到 497 mL（约 30.4 s）；修正后两次独立启动从第 2 次起均在 475..525 mL 内（约 10.3 s）。首次进入 490..510 mL 的结果时间分别约 10.3 s 和 18.4 s，后续仍有约 488..518 mL 波动，不能解读为每次均满足 ±1%。时间从 run 命令到完整周期结果接收，包含初始呼气及呼气结束后发布的等待。测试结束均收到停止确认，未覆盖其他管路、PEEP 或暂停比例。

VAC 供气段压力前馈使用 `Pff = PEEP + Vref*deliveryTarget/userTarget/30 + 0.1572*Qref + 0.004013*Qref*Qref`，压力和流量单位分别为 cmH₂O 与 L/min。30 mL/cmH₂O 是当前模拟肺 250/500/750 mL RTT 数据统一使用的台架顺应性；不能用潮气量反推顺应性，否则高 VT 会被错误解释为更软的肺。`PHASE_REF_VOLUME` 是现有分段流量上升曲线累计面积按整个供气窗口归一化后的用户目标容量轨迹（mL），吸气入口为零、供气结束为用户 VT，支持短于上升沿的供气窗口。控制器按 `deliveryTarget/userTarget` 将外环补偿同步映射到弹性压力前馈，避免只提高流量参考却在吸气末段缺少相应压力。模型保留低增益 PI、绝对压力/转速上限和 VTI 限幅反馈；暂停段继续使用原有实测压力前馈与泄漏流量控制。

VAC 吸气暂停继续使用近端流量反馈，不锁定患者压力。入口按患者压力选择 Kp：低于 30 cmH₂O 为 0.003，高压为 0.001；Kd=0.00005、Ki=0 制动供气尾段；至少经过 120 ms，且近端流量下降到泄漏目标上方 2 L/min 以内后，开启 Ki=0.02 的积分补偿。此时按患者压力一次性选择稳定段参数：低于 30 cmH₂O 保留 Kp=0.003、Kd=0.00005；达到或超过 30 cmH₂O 使用 Kp=0.0003、Kd=0，减少高压力工况的反馈振荡。本次暂停内不反复切换增益。暂停风机指令每 6 ms 最多变化 40 个指令单位，绝对压力对应转速上限优先于变化率限制，限幅时撤回同方向积分增量。患者侧漏气才计入近端流量目标，呼气阀侧流量不能直接当作患者侧漏气目标。验证重点是暂停稳定后 patflow 相对泄漏补偿目标的偏差与振幅，不以原始过零次数作为验收标准。

`TECH_ALARM_CPAP_TOO_HIGH` 已启用：以当前呼吸计划 `peepCmh2o` 为基准，`INSP_REAL_PRS` 与 `PAT_REAL_PRS` 同时严格大于 PEEP + 15 cmH₂O 持续 15 s 触发；报警后两路同时严格小于 PEEP + 14.5 cmH₂O 持续 3 s 恢复。等于阈值或任一路不满足条件会中断对应计时，吸呼气切换不清计时。停机、零点补偿阶段清除状态；计划不可用时中断计时并保留报警状态。由 AlarmTask 在临界区获取计划及两路压力快照。`develop/test_monitor_leak.py` 包含阈值、计时边界、中断、阶段切换及 tick 回绕回归。

`stVentPatientSettings.useHostSettings` 默认 0，使用本机固定设置；1 使用独立的 MCM 设置副本。两种情况下均接收并缓存参数；MCM 报警限（0xAC）不受此开关限制，在 VentTask 中按 scale 解码并写入当前报警设置，未下发的字段保留原值，切换参数源时重新应用已缓存的报警限。压力和呼出潮气量检测器直接读取这些设置；窒息时间供 CPAP/PSV 窒息后备使用，PSV-ST 周期机控使用自身频率。通气命令 1 启动、0 停止。已绑定 PAC/VAC/CPAP-PSV/PSV-ST 的现有字段，其他模式仅缓存，由 Scheduler 拒绝启动。时间按 scale 解码为秒后转为毫秒。波形沿用压力 ×10、流量 ×10+2000、容量 mL 和毫秒时间戳；相位 1 吸气、2 呼气。CommTask 优先级 5、周期 10 ms、栈 1024 words，低于通气和传感器任务。

本机参数模式保留 Scheduler 当前选择的通气模式；上电尚未选模式时，MCM 启动命令默认使用 PAC。上位机参数模式需先收到模式字段才能启动。触发选择按 `m_assistTrig=0` 关闭、`m_FlowTrigger=1` 流量触发／0 压力触发绑定。115200 波特率及上述物理单位、相位值需在上位机实机联调时核对。`develop/test_protocol.py` 使用真实协议源码和模拟串口验证分包、CRC、缓存、参数源切换、启停、ACK 和波形编码。

CommTask 独占 PA9/PA10 串口和协议队列，SysTask 恢复 20 ms 空任务。每个 CRC 有效的 MCM `0x7F` 心跳收到后返回一次 `FE FF 7F 01 00 + CRC`；保留原应答字节，不为心跳回复注册 ACK 超时重发。请求带 ACK 标记时，另保留通用原帧回显应答。回复进入高优先级队列，队列满时保留待应答计数。RTT 每 5 s 输出 `heartbeat rx/tx/pending/overflow/online`；tx 表示已交给串口发送，不能单独证明上位机收到。连续 5 s 无有效 MCM 帧时 online 为 0，新帧可恢复。

2026-09-07 心跳实机自测：经 Device Tool 编译、烧录校验及 RTT 观察，清除了 main 入口残留硬件断点，并为 RTT 指定 ELF 中的控制块地址。`build/heartbeat_final_rtt.log` 连续报告 rx/tx 相等（最终 65/65），pending=0、overflow=0、online=1；MCM 心跳约每秒一次。tx 为设备串口提交计数，此记录不包含 MCM 界面确认。主机协议回归另验证 1110 次应答及背压、断连恢复。

`MONITOR_HMI_PRS_MEAN` 在每次呼气结束、下一次吸气开始前结算，单位 cmH₂O；固定 6 ms 累加本次吸气（含暂停）和呼气的全部 `PAT_REAL_PRS`，除以样本数，不受流量方向或死区影响。结果与 `stBreathResult.meanPressureCmh2o` 同步发布，下一次结算前保持不变；停止、调零或无有效计划时清零。任一压力样本非有限或累计无效时不置 `BREATH_RESULT_VALID_MEAN_PRESSURE`，监测值置零，该次平均压不上传。CommTask 沿用逐呼吸上传与队列重试入口，通过 MCM 监测参数 `0x03` 发送有符号平均压 ×10；`test_monitor_leak.py` 和 `test_protocol.py` 覆盖整周期均值、结算时序、周期隔离、异常值与正负平均压报文编码。

流量气体补偿：`PAT_REAL_FLOW`、`INSP_REAL_FLOW`、`O2_REAL_FLOW` 在校准处理阶段统一乘以 `BTPS_COEFFICIENT`。当 `GetVentPatientSettings()->Gas == VENT_GAS_BTPS` 时，系数按 SFM3119 的 20°C、1013 hPa 干气参考，以 `1013/(1013-62.66)*310.15/293.15` 计算，其他气体类型为 1。患者流量先对滤波 ADC 做零漂和密度补偿、查表，再乘系数；零点偏移接口使用当前气体条件的 L/min，内部保存补偿前偏移，切换气体类型不改变传感器零点。

2026-09-08 时间补偿测试肺自测：经 Device Tool 构建、烧录、RTT 采集，PEEP 5 cmH₂O、设定 Ti 2000 ms、频率 15/min、暂停 0%，300/500/800 mL 各记录 15 次完整呼吸，全部符合 ±(10 mL + 目标×5%)，且无容量限幅标志。末尾五次 VTI 分别为 295.39～303.82、496.89～503.01、783.48～805.41 mL，均值分别为 300.518、499.800、791.908 mL；三个设定均从首个完整呼吸进入验收范围。流量设定分别固定在 9/15/24 L/min。原始记录及验收见 `build/vac_time_300/`、`build/vac_time_500/`、`build/vac_time_800/`，汇总为 `build/vac_time_summary.json`。测量使用设备近端 VTI，未使用独立流量分析仪；结论仅覆盖上述测试肺工况。测试结束已停止通气。

2026-09-08 短吸气时间回归：氧浓度 21%、PEEP 5、频率 20/min、设定 Ti 500 ms、暂停 0%、触发关闭。原 100 ms 相同风机目标重发间隔下，400 mL 出现 353.22～448.04 mL，600 mL 出现 544.16～645.72 mL，均未通过。400 mL 波形显示，部分呼气入口风机降速滞后，正向尾流为 61.68～115.09 mL。将 `BLOWER_VCM_CONTROL_KEEPALIVE_MS` 改为 10 ms 后，尾流收窄为 60.60～76.05 mL；此对比支持缩短重发窗口，未单独证明驱动器内部丢弃首帧的原因。双帧 DMA、230400 波特率及原压力/转速限值保持原配置。

同一修复固件上，200/400/600 mL 各测 30 次完整 VAC 呼吸，全部符合 ±(10 mL + 目标×5%)，全程范围分别为 190.10～212.05、375.82～410.14、570.81～612.65 mL；末尾五次均值为 200.940、403.012、600.824 mL。流量分别固定为 24/48/72 L/min，计划供气时间范围为 500～546/500～525/500～526 ms，沿用 ±20 ms 逐呼吸时间步长。波形中的监测 Ti 含切换后的正向尾流，不能当作 Scheduler 计划时间。原始记录在 `build/vac_short_200_v2/`、`build/vac_short_400_v2/`、`build/vac_short_600_v2_run/`，汇总为 `build/vac_short_summary.json`。600 mL 期间 CRC、帧尾、UART、DMA、RX 溢出计数均为零；TX busy 计数非零，不能表述为从未发生发送竞争。所有容量值来自设备近端 VTI，结论仅覆盖当前测试肺工况。

同固件补测 500 mL、Ti 2000 ms、15/min：10 次完整呼吸全部通过，范围 495.83～524.54 mL，末尾五次均值 500.994 mL，记录在 `build/vac_long_500_v2/`。

PAC 压力控制不使用报警高限 `pressureHigh` 限制患者压力参考值或吸气压力目标；仍保留控制器固有的 100 cmH₂O 目标上限。高压报警检测继续使用 MCM 下发的报警高限。CPAP-PSV/PSV-ST 和 VAC 保持原有限压逻辑。`develop/test_flow_pause.py` 覆盖 PAC 报警高限变化不影响目标及 PSV/ST 限压回归。

`MONITOR_HMI_MV_LEAK` 为完整呼吸周期内 `MONITOR_LEAK_FLOW` 的 6 ms 等间隔均值，单位 L/min（等价于周期泄漏积分除以周期时长），包含吸气、暂停和呼气。周期结束发布到 `stBreathResult.minuteLeakLpm`，周期内保持不变；首个周期尚无有效泄漏系数、周期内估计无效或压力异常时置零且不置 `BREATH_RESULT_VALID_MINUTE_LEAK`。停止、调零和无有效计划时清零。CommTask 按逐呼吸结果通过 MCM `0x0B` 上传，无符号 16 位、数值 ×10、scale=1，沿用现有队列重试。监测与协议主机回归覆盖周期均值、快照保持、异常窗口和报文编码。

`MONITOR_HMI_MV_TOTAL` 按同一完整周期的呼出 VTe（mL）×60/实测周期时长（ms）计算，单位 L/min，包含自主和机控呼吸，不另加跨周期平滑。`MONITOR_HMI_LEAK_PERCENT = MONITOR_HMI_MV_LEAK / (MONITOR_HMI_MV_TOTAL + MONITOR_HMI_MV_LEAK) × 100`，随完成结果发布。容量采样或泄漏估计无效、分母非有限或为零时，泄漏率置零且不上传；有效的零泄漏上传 0%。MCM `0x0C` 使用一字节整数百分比、scale=0，截断小数并限幅 0..100；沿用逐呼吸上传与队列重试。停止、调零时监测快照清零。

`MONITOR_HMI_RES_INSP = 60 × (Ppeak - Pplat) / Qinsppeak`，`MONITOR_HMI_RES_EXP = 60 × (Pplat - PEEP) / Qexppeak`，流量使用近端 `PAT_REAL_FLOW`（L/min），呼气峰值取呼气阶段负流量最大绝对值；PEEP 使用完成结果的呼气末五点滑动平均压力。吸气气阻要求同口有效峰压和平台压；峰压与峰流量分别取最大值，变流量模式下仍为近似估算，平台压采样条件沿用现有实现。两项随完整呼吸结算并保持到下次发布，吸气和呼气公式均乘 60，将 L/min 换算为 L/s。非有限采样、缺少对应压力、零峰值或负/非有限计算结果时置零且不上传；停止、调零时清零。CommTask 从同一 `stBreathResult` 快照上传 MCM `0x16` / `0x17`，沿用无符号 16 位、scale=0 的协议定义（截断小数、限幅 0..65535）及队列重试。主机回归覆盖公式、峰值相位隔离、跨周期清零、无平台压、异常采样和报文编码。

`MONITOR_HMI_C_DYNC = VTi / (Ppeak - PEEP)`，`MONITOR_HMI_C_STAT = VTe / (Pplat - PEEP)`，单位 mL/cmH₂O；使用同一完整呼吸的容量、压力和呼气末 PEEP，通过 `stBreathResult.complianceDynamic` / `complianceStatic` 同步发布并保持到下次结算。压差非正或非有限、容量为负、缺少对应有效压力/容量或周期采样异常时置零且不上传；停止、调零时清零。MCM 静态顺应性为 `0x18`，动态顺应性为 `0x19`，均使用无符号 16 位、数值 ×10、scale=1，沿用逐呼吸上传和队列重试；主机回归覆盖公式、无平台压、零/负压差、异常采样、快照保持、清零和报文缩放。

近端流量校准参考为开放管路下 SFM3119 未乘 BTPS 的原始 SLM（20°C、1013 hPa），不是运行期患者侧实际体积流量。保留既有 14 Hz ADC 滤波；`PAT_REAL_FLOW` 查表前先减运行期 ADC 零漂，再围绕表内反插值的真实零流量 ADC 乘密度比 `1 + PAT_REAL_PRS / CALIBTRANS_AMBIENT_PRESSURE_CMH2O`。患者压力来自同轮 Butterworth 压力换算。密度比在差压域应用后再查原表，正负方向均使用自身曲线，不使用固定 VTe 比例。此模型假定 ADC 与差压线性、校准与运行温度/气体黏度近似相同、开放标定压力接近大气；环境压力暂按 1013 hPa，无气压或近端温度实测补偿。

调零接口继续接收“当前累计值 + 实测残差”，通过反查同一校准表将残差换成 ADC 偏移；getter 是累计调零标记，不能作为恒定流量误差扣除。气体模式切换不改变 ADC 偏移。校准不可用或换算非有限时发布 NaN，供监测拒绝该周期。EEPROM 表及潮气量积分定义不变。`develop/test_flow_conversion.py` 使用真实换算/数据处理源码，覆盖非对称非线性表、双向零漂、重复调零、SFM3119/BTPS 基准、压力密度补偿及异常恢复。

启动调零需要连续 1000 ms 满足入口流量及患者压力的独立静息门限，并要求近端流量样本为有限值；任一条件中断即重新计时，3000 ms 超时保留旧零点。近端流量的绝对值不再作为准入门限，避免超过旧 ±10 L/min 门限的零漂反向锁死补偿。补偿写入失败会中止本次启动并记录错误，不再误报成功。此窗口用于降低滤波后相关零点噪声对整周期容量的偏置，不根据 VTi/VTe 差值自动学习零点。`test_flow_zero_offset.py` 验证大零漂、写入失败、完整窗口前不更新以及超时保留。

2026-09-08 潮气量换算台架回归：模拟肺 PAC，PEEP 5、Delta-P 25 cmH₂O、Ti 1350 ms、20/min、21% 氧、触发关闭。统一排除前两次启动呼吸并取随后 13 次：旧版平均 VTe−VTi 为 22.80 mL（3.34%）；最终 1000 ms 调零固件两次分别为 12.69 mL（1.78%）和 20.06 mL（2.84%），最大单次差值分别为 21.76、28.45 mL，未消除误差。原始日志/结果在 `build/volume_balance_old/`、`build/volume_balance_zero_long/`、`build/volume_balance_zero_repeat/`；统一统计、固件及源码 SHA-256 在 `build/volume_balance_comparison.json`。VAC 500 mL、PEEP 5、Ti 2000 ms、15/min、无暂停的末五次 VTI 为 495.93..503.00 mL，通过原 ±35 mL 目标验收，记录在 `build/volume_balance_vac/`。其中末次 VTi/VTe 为 500.02/533.09 mL，同时 PEEP 降到 3.48 cmH₂O，提示周期首尾储气变化仍影响两者比较；不能把容量目标通过解读为每次吸呼气相等。全部经 Device Tool 构建/烧录/RTT，最终收到停机确认；仅设备自身测量，无独立呼气参考仪器。

CPAP/PSV 使用 `gVentCpapPsvSettings`，通过 `vt psv` 或 `breathSchedulerStart(VENT_MD_CPAP_PSV)` 启动。压力/流量触发后以 PEEP + pressureSupportCmh2o 为目标，按峰值流量百分比切换呼气，保留最短吸气、最大吸气及最短呼气保护；无患者触发达到公共 `apneaTimeAlarm`（秒）时触发窒息报警，并在呼气捕获及最短呼气保护通过后启动后备通气。后备目标为 PEEP + `apneaPressureCmh2o`，频率为 `apneaRateBpm`，吸气时长为 `apneaInspTimeMs`，上升时间复用 `riseTimeMs` 并限制到后备吸气时长。两类呼吸的压力上限统一取公共 `pressureHigh`，CPAP/PSV 结构体不再保留 `pressureLimitCmh2o`、`apneaAlarmTimeMs`。Scheduler 校验后备压力、频率及吸气时序，确保保留最短呼气时间。

窒息引擎在 VentTask 中使用公共 `apneaTimeAlarm` 计时，后备呼吸不会解除窒息报警；等待后备和后备期间由 AlarmTask 的 `physalarmapnea.c` 发布 `PHYS_ALARM_APNEA`，映射 0xAD 的 `APNEA_ALARM`（bit 8）。后备期间 Trigger Engine 仍每个 VentTask 周期执行，在允许触发的呼气阶段检测患者努力；有效压力/流量触发优先于后备请求，恢复压力支持并清除窒息状态、重新计时。停机及非通气阶段清除状态。旧协议 0xAF 的 0x0C/0x16/0x19 分别绑定 PSV 后备压差、频率及吸气时间，0x29 峰压不再写入 PSV，压力上限只从公共报警限值接收。

每次吸气（包括启动等待后的第一口）均加载新的计划序号，启动等待呼气的计划不能复用于第一口吸气。否则吸气结束清除 capture 标志后，呼气控制器因序号未变停留在 PEEP，不再通知捕获完成，CPAP/PSV 第二次触发永久被挡住。`test_vti_compensation.py` 使用真实呼气控制器及相位/触发/流量切换引擎验证压力、流量两类各连续三口，无手动 capture 通知；修复前在呼气准备标志恢复处失败，修复后通过。

报警接口仅供任务上下文调用：AlarmTask 初始化并更新检测状态，CommTask 通过临界区读取当前状态。内部 `ePhysAlarmType` 是统一检测器注册编号，不是线上 bit 编号。`alarmbits.h` 中各枚举值才是对应主 ID / SubId 的 bit 编号；位域遵循目标 ARM GCC 小端 ABI，发送按整数值显式小端编码。

MCM 报警每 500 ms 全量发送，不做变化抑制，也不锁存已恢复的短时事件。`0xAD` 固定 4 字节；`0xAB` 每次包含 SubId 0..4，宽度依次为 4、4、1、1、1 字节，包括全零模块与校准模块。未实现项及保留位为 0。已实现的气道压力高/低、潮气量高/低分别对应 `0xAD` bit 0、1、4、5；PEEP 高/低、持续气道压过高对应 `0xAB/0` bit 0、1、4。氧气供应不足预留映射至 `0xAB/1` bit 30，`0xAB/0` bit 12 保持保留。PEEP/CPAP 检测迁移至 `app/techalarm/techphys.c`，原阈值和恢复时序保持一致。

技术报警与生理报警由 AlarmTask 每 10 ms 使用同一 nowMs 分别调用 manager。`techAlarmManagerInit()` 在处理开始前初始化，`Process()` 及各检测器仅由 AlarmTask 调用，`StateGet()` / `SnapshotGet()` 允许其他任务通过临界区读取，不用于 ISR。注册表显式维护 enabled、检测函数和协议模块/位号；未知报警类型返回 false，空快照指针忽略。PEEP 高/低和 CPAP 默认启用，其余检测器仍为关闭的占位项，不根据现有硬件数据新增判断。氧气供应不足属于 `techdevice`，保持 SubId 1 bit 30。新增检测应在对应分组实现，并在 `techalarmmanager.c` 注册、设置启用标志及位号；宏和运行类型位于对应头文件。协议继续通过 `physalarm/alarmbits.h` 共享位图定义。

PEEP 动态监测在呼气阶段保留最近 5 个有限的 `PAT_REAL_PRS` 样本，每 6 ms 发布均值；不足 5 点按实际点数平均，无有限压力时结果为 0 且无效。`MONITOR_DYN_PEEP`、`MONITOR_DYN_PEEP_VALID` 及报警触发/恢复逻辑保持实时语义，`stBreathResult.peepCmh2o` 在呼气结算时保存最终均值，供气阻/顺应性计算。

显示独立使用 `MONITOR_HMI_PEEP` / `MONITOR_HMI_PEEP_VALID`：PAC 仅在呼气结束、下一吸气开始前更新，周期内保持上一结算值。CPAP/PSV 与 PSV-ST 在呼气捕获完成后，连续 17 个有限压力点（约 100 ms）保持在跨度 0.2 cmH₂O 内后，以最近 5 点均值持续刷新；超出该稳定区间则从当前点重新确认，包括首次触发前及长时间等待；泄压、非稳定窗口和吸气阶段保持上一显示值，呼气结算不以触发努力期间的压力覆盖它。捕获有超时路径，因此不能只凭 ready 判定稳定；稳定窗口不按设定 PEEP 限幅，真实稳定高压仍可显示。其他模式沿用实时窗口显示。首次有效显示前不上传 PEEP；停止、调零或计划无效时清除显示和有效位。显示值及有效位通过 RTOS 临界区同步发布，协议按原发送节拍读取该快照，不依赖完整呼吸序号。`vt peep` 只读输出同一时刻的计划、相位、ready、患者压力、动态 PEEP、显示 PEEP 及有效位，压力字段放大 100 倍，供 Device Tool RTT 台架核对。

2026-09-14 PEEP 显示时序模拟肺复测：最终固件经 Device Tool 构建、烧录校验及 RTT 采集。PAC 使用当前本机参数 PEEP 5、Delta-P 25、Ti 800 ms、15/min，约 71 s 的 100 ms 诊断快照中显示为 4.61..4.75 cmH₂O，显示变化仅出现在新吸气计划，未记录到同一周期内跳变；56 个呼气动态压力不低于 6 的快照仍保持显示低于 5。PSV 首次触发前等待可持续刷新，10 次人工辅助触发均未记录到吸气内或未捕获呼气内的显示变化；含人工扰动的显示总范围为 4.45..5.82，最后回到 4.75。初版仅 5 点稳定确认曾被触发扰动的短暂平台误导，最终改为连续 17 点确认。原始日志、源码/固件 SHA-256 和汇总见 `build/peep_timing/`；`rtt_run1.log` 为初版，`rtt.log` 为最终版，停止命令已确认。主机监测、协议及真实调度/相位回归均通过。日志从启动阶段起另有 SFM3119 读取错误，未在本次修复中处理；本记录只验证压力显示时序，不能作为流量测量或整机正常的验收。未采集实体屏幕及独立压力分析仪数据。Device Tool 普通复位落入旧的 0x08000000 启动向量，实测由用户辅助调试启动 0x08010000 应用，未修改引导区。

PSV-S/T 使用 `gVentPsvStSettings.inspRateBpm` / `inspTimeMs` / `maxInspiratoryTimeMs`：患者压力或流量触发生成压力支持呼吸，目标为 PEEP + pressureSupport，上升时间为 riseTimeMs；上升段和最短吸气保护结束后，流量连续 3 点不高于本次峰值的 cycleOffPercent 时切换呼气，最大自主吸气时间提供兜底。无患者触发时，从启动调零后的初始呼气或上次实际吸气开始计时，达到 `60000 / inspRateBpm` 启动同一压力目标的机控呼吸，按 inspTimeMs 时间切换。确认的患者触发先于同 tick 定时处理，重新开始周期计时；机控呼吸不参与流量切换。ST 定时不依赖 apneaTimeAlarm，也不等待患者触发用的呼气捕获，但仍要求呼气阶段和最短呼气时间。`APNEA_ENGINE_TIMED` 表示正常 ST 机控，不触发窒息报警；CPAP/PSV 原有窒息后备逻辑保留。

ST 校验要求频率 1..160/min，机控和最大自主吸气时间均至少为最短吸气时间，并各自在周期内留出最短呼气时间；最大自主吸气时间不超过 10000 ms，上升时间不超过两类吸气时间。无效设置不替换已应用计划。运行中有效修改在下一呼吸计划生效。MCM `0x14` / `0x17` / `0x18` 分别写入 ST 频率 / 机控吸气时间 / 最大自主吸气时间；`0x16` / `0x19` 仅用于 CPAP/PSV 窒息后备参数。

RTT 输入 `vt psvst` 启动 PSV-S/T，沿用当前 local/host 参数源，并输出 `VT_PSVST_SETTINGS`；`vt status` 查看设置和实际呼吸类型，`vt stop` 停止。当前本地默认 PEEP 5、支持压力 10 cmH₂O、15/min、机控吸气 1300 ms、最大自主吸气 2000 ms、上升 200 ms、流量切换 25%。`develop/test_vti_compensation.py` 覆盖 ST 周期边界、连续机控、压力/流量自主恢复、同 tick 优先级、流量切换、最大吸气、tick 回绕和参数拒绝；仅为主机逻辑回归，未做实机气路验证。

P-SIMV / V-SIMV 通过 `GetVentPSimvSettings()` / `GetVentVSimvSettings()` 选择本地或主机参数，RTT 命令 `vt psimv` / `vt vsimv` 启动，`vt trigger` 支持两种模式。`SIMVRateBpm` 决定机控周期，P-SIMV 的 `inspiratoryPressureCmh2o` 是高于 PEEP 的压力差；V-SIMV 常规潮气量使用 `tidalVolumeMl`，吸气暂停使用 `inspPausePct`，容量补偿只学习常规容量机控呼吸，不学习压力支持或窒息后备。

SIMV 触发窗位于机控到期点前：成人 5000 ms，小儿及婴幼儿 1500 ms，上限为设定机控呼气时长；结构体 `syncWindowMs` 保留兼容但不覆盖上述规则。窗内患者触发输送一次对应 AC 机控呼吸，并以实际吸气起点重启机控周期；未触发则到期补发，定时补发不等待呼气捕获，但保留最短呼气 192 ms。窗外触发使用压力支持，支持压力为零时维持 PEEP；支持呼吸不重置机控计时，最大吸气 2000 ms，并为机控到期点预留最短呼气时间。启动仍沿用调零后初始呼气流程；相位、触发和参数应用接口只在既有任务上下文使用，不用于 ISR。

SIMV `apneaSwitch` 使用 `eVentApneaType`：`VENT_APNEA_OFF=0`（默认关闭）、`VENT_APNEA_PRESSURE=1`（`apneaPressureCmh2o` 生效）、`VENT_APNEA_VOLUME=2`（`apneaVolumeTidalMl` 生效）。P-SIMV 和 V-SIMV 均可选择任一后备类型，只校验所选后备目标；无患者触发达到公共 `apneaTimeAlarm` 时进入所选后备，普通定时机控不清除无自主呼吸计时。后备使用 `apneaPressureCmh2o` / `apneaVolumeTidalMl`、`apneaRateBpm`、`apneaInspTimeMs`，后备频率不得低于 SIMV 频率；患者触发恢复常规 SIMV，关闭后备后下一计划恢复常规机控。主机映射包括参数 0x15 SIMV 频率、0x0E 常规潮气量、0x0F 后备潮气量及开关组 0x0A 后备选择（0/1/2 对应上述枚举；主机需发送所选类型，不能仅用布尔值区分两种后备）。参数变化在下一呼吸应用，当前计划保持不变。

`develop/test_vti_compensation.py` 增加真实 Scheduler/Phase/Cycle/Apnea 的 SIMV 回归，覆盖三类患者、窗口边界、短呼气全窗、窗外支持不延误机控、计时回绕、后备进入/恢复/关闭、参数拒绝与容量反馈隔离；`test_trigger.py` 验证两种模式压力/流量触发及关闭；`test_protocol.py` 验证 SIMV 参数和后备开关映射。主机回归不代表已完成模拟肺或实机气路验证。

2026-09-14 PAC 升压超调修复：将工作区被改为 12.5 的 `PRESSURE_CONTROLLER_OUTER_KP` 恢复为 0.5。原值在患者压力跟踪误差达到 0.8 cmH₂O 时即触及 +10 cmH₂O 修正上限，并与流量前馈、升压提前量叠加；当前测试肺基线每次升压平均顶限 160.5 ms。恢复后两次独立启动均未出现正向顶限。平台增益、吸气目标与时长未调整。

台架工况为 PEEP 5、Delta-P 25（目标 30 cmH₂O）、Ti 800 ms、15/min、21% 氧、BTPS、触发关闭；排除前两次启动呼吸。基线 8 次呼吸平均/最高峰压 31.746/32.20，修改版两轮各 12 次为 30.858/31.07、30.675/30.83 cmH₂O，平均超调降低约 51%/61%；第一轮修改版仅取前 8 次平均峰压仍为 30.87。末尾 240 ms 平均压力从 29.717 变为 30.258/30.219 cmH₂O。仍有小幅超调，不表示完全消除，也未验证其他肺顺应性、阻力或漏气工况。复测控制 Ti 为 804 ms，连续采样和压力检查通过，最终收到 `stop status=1`。

记录位于 `build/pac_20260914_baseline/`、`build/pac_20260914_fixed/`、`build/pac_20260914_verify/`（RTT、CSV、固件/源码哈希、`summary.json`、`overshoot.json`），重算入口为 `build/pac_20260914_compare.py`。前两轮旧采集程序在完成并停机后因短平台校验失败，原始波形完整，统计已离线重算；第三轮使用修正后的采集程序完整通过。`develop/test_pac_rtt.py` 现在按升压入口至参考下降切分完整吸气，新增全吸气峰压/超调统计，并将峰压纳入原 32 cmH₂O 台架检查上限，避免只检查后半段遗漏早期过冲。编译、烧录、复位和 RTT 均经 Device Tool；`test_flow_pause.py` 主机回归通过。

2026-09-15 PAC 高压力模拟肺回归：Device Tool 构建、烧录和 RTT 采集，氧浓度 21%、PEEP 5、频率 25/min、Ti 800 ms、rise 设置 200 ms、触发关闭。DeltaPressure 15/25/35/45 每组采集 44 秒，剔除两次启动呼吸后各有 15 次有效呼吸；6 ms 波形连续、参数回读一致、控制 Ti 均为 804 ms。原有参考升压限速保持，四档有效参考上升时间分别约 300/250/350/450 ms，不能把设置的 200 ms 当作四档实际参考上升时长。

| DeltaPressure | 目标压力 | 修复前末段平均峰峰值 | 修复后末段平均峰峰值 | 修复后末段压力范围 |
|---|---|---|---|---|
| 15 | 20 | 0.264 | 0.281 | 19.89～20.34 |
| 25 | 30 | 0.799 | 0.512 | 29.72～30.59 |
| 35 | 40 | 2.638 | 1.563 | 39.02～41.08 |
| 45 | 50 | 4.942 | 2.236 | 48.95～51.55 |

压力单位均为 cmH₂O，末段为参考下降前留出 12 ms 的 240 ms 窗口；峰峰值包含慢趋势，不等同于纯振荡幅度。原始基线在 `build/pac_osc_before_{15,25,35,45}/`，最终固件数据在 `build/pac_osc_tuned_{15,25,35,45}/`，汇总为 `build/pac_osc_comparison.json`，第六次呼吸对比为 `build/pac_osc_tuned.png`。各最终数据目录保留原始 RTT、波形 CSV、逐呼吸统计和源码/固件 SHA-256。高压快速反复波动明显减弱，但 45 档仍有最高 1.55 cmH₂O 过冲；结果仅覆盖当前测试肺，压力来自设备传感器。主机回归 `test_flow_pause.py` 通过；RTT 验证非法定时拒绝及两参数 `vt set` 保留定时。结束时已停止通气并将 PAC 参数恢复为 PEEP 5、DeltaPressure 25、Ti 800、rate 25、rise 200。

2026-09-15 PAC PEEP 10 平台爬升修复：同一模拟肺，PEEP 10、DeltaPressure 25、rate 15/min、Ti 800 ms、rise 设置 200 ms、触发关闭。健康采集基线为 `build/pac_flat_reset_p10/`（11 次有效呼吸），最终固件为 `build/pac_flat_final_p10/`（68 秒、14 次有效呼吸）。平台窗口剔除 HOLD 开始 60 ms 和吸气末尾 12 ms，比较窗口首尾各 60 ms 均值；平均前后压差由 2.819 降至 0.431 cmH₂O，最差绝对压差由 2.972 降至 0.596，平台平均峰峰值由 3.055 降至 1.197，末段 240 ms 平均峰峰值由 1.618 降至 0.559，全吸气最大压力由 36.48 降至 35.52。参考目标为 35，平均早段 34.515、末段 34.946，仍有小幅中段下凹；不将前后均值相近等同于无波动。连续采样、参数回读、流量信号有效性及每口绝对压差≤0.8 的台架检查均通过，控制 Ti 804 ms。对比图为 `build/pac_flat_comparison.png`，汇总为 `build/pac_flat_comparison.json`。

初次采集 `build/pac_flat_before_p10/` 出现供气流量读数冻结，历史 RTT 同时有 SFM3119 读取错误；经 Device Tool 复位后恢复。上述修复前后比较使用复位后的健康信号，未将冻结读数用于调参结论，也未修改传感器驱动。新增 RTT 验证会拒绝患者流量明显变化而供气流量整口不变的采集。结果限于当前测试肺和设备传感器，尚未覆盖全部阻力、顺应性及 PEEP 工况。

保留先前优化的回归：最终固件 `build/pac_flat_final_{15,25,35,45}/` 在 PEEP 5、rate 25/min、Ti 800 ms、rise 200 ms 下各采集 44 秒、分析 15 次呼吸。末段 240 ms 平均压力峰峰值对比如下：

| DeltaPressure | 此前优化版本 | 本次最终版本 |
| --- | ---: | ---: |
| 15 | 0.281 | 0.298 |
| 25 | 0.512 | 0.576 |
| 35 | 1.563 | 1.785 |
| 45 | 2.236 | 2.157 |

单位 cmH₂O。四档均通过既有压力范围、连续采样、参数回读和控制 Ti 检查；35 档此次峰峰值较历史采集增加 0.222，45 档减少 0.079，不能声称实测波形完全一致。新补偿在 PEEP≤5 的权重为零；额外将修改前保存的控制器与最终代码输入相同的 72 口脚本传感器序列，覆盖 PAC PEEP 1/5、PSV/ST PEEP 5/10/15 和总目标 20/30/40/50，9,648 个周期的执行器输出与压力诊断逐字节相同，结果存于 `build/pac_flat_equivalence.json`。主机 `test_flow_pause.py` 通过（含 PEEP 插值、前馈捕获、跨呼吸重置与模式隔离）。回归对比为 `build/pac_flat_regression.json` 和 `build/pac_flat_regression.png`，最终五组采集的源码/固件 SHA-256 均与当前文件一致。构建、烧录完成，结束时已停止通气并恢复 PEEP 10、DeltaPressure 25、Ti 800、rate 15、rise 200、触发关闭，记录见 `build/pac_flat_final_state.log`。

2026-09-15 PAC PEEP 15 后半段凹陷排查：沿用 DeltaPressure 25、rate 15/min、Ti 800 ms、rise 200 ms、触发关闭，总目标 40 cmH₂O。经 Device Tool 下载当前工作区固件后，`build/pac_dip_baseline_p15/` 的 11 口有效呼吸复现填充结束后的减速下凹，后半段最低 37.65。修复在现有流量前馈之上添加基于实际转速的有界减速缓冲；只对距离吸气结束至少 200 ms 的提前填充结束启用，避免将 PEEP 10 临近呼气的正常减速阻止。未保留实验中的 PI 增益变化。

最终源码的首次 PEEP 15 采集 `build/pac_dip_gated_p15/`（56 秒、11 口有效呼吸）：后半段最低 39.01，全平台平均峰峰值由 2.775 降至 1.551 cmH₂O，平台前后平均压差由 -1.042 变为 -0.026，最高压力由 40.77 变为 41.06。仍有小幅波动和短暂反向流量：后半段平均反向量 7.616→7.083 mL，最负瞬时流量 -9.08→-9.82 L/min，不能声称消除反向流。该次“全平台最低不低于目标减1”附加检查未通过，原因是吸气后 318 ms 的升压收尾最低 38.89；后半段最低为 39.01，两者分别记录，未放宽原检查阈值。对比图和 JSON 见 `build/pac_dip_comparison.*`，原始摘要保留失败标志。

保留行为验证：`build/pac_dip_equivalence.py` 将相同传感器序列输入本次修改前保存的控制器与最终控制器，PAC PEEP 1/5、PSV/ST PEEP 5/10/15、PAC PEEP 10 临近吸气结束才降流，共 96 口、12,864 周期，执行器请求和压力诊断逐字节相同，结果见 `build/pac_dip_equivalence.json`。`test_flow_pause.py` 通过，包括缓冲方向/限幅、压力门控、稳定后消退、无效转速恢复和临近呼气不启用缓冲。

同一最终固件回归 `build/pac_dip_gated_p10/`：PEEP 10、ΔP25、rate15、Ti800，11 口有效呼吸，平台前后平均压差 0.353（此前 0.431），末段平均峰峰值 0.565（此前 0.559），最高压力 35.37（此前 35.52），原平坦度检查通过。PEEP 5 原四档分别采集 44 秒、15 口有效呼吸，末段平均峰峰值为 0.265/0.579/1.935/2.216，此前为 0.298/0.576/1.785/2.157 cmH₂O；四档均通过既有压力范围、连续采样、参数与控制 Ti 检查，实测波形存在差异，未声称逐点一致。对比为 `build/pac_dip_regression.*`，原始记录为 `build/pac_dip_gated_{15,25,35,45}/`。

延长吸气验证 `build/pac_dip_gated_long/`：PEEP15、ΔP25、rate15、Ti2000，11 口有效呼吸，控制 Ti 2004 ms，末段压力 39.42～40.44，末段平均峰峰值 0.533，全平台前后平均压差 -0.389，最差绝对压差 0.690；既有压力范围与0.8压差检查通过。全平台最低 38.88（吸气后768 ms），因此额外的全平台下凹≤1检查未通过，保留该失败结果；后半段最低39.38。该结果仅验证当前测试肺，未证明全平台达到±1或全部阻力/顺应性工况适用。

最终七组采集的源码/固件 SHA-256 与当前文件一致。Device Tool 构建、下载完成，文件结构和主机回归通过。结束时恢复 PEEP15、DeltaPressure25、Ti800、rate15、rise200、触发关闭并停止通气，记录见 `build/pac_dip_final_state.log`；已恢复暂挂的 Ozone 进程。

VAC 暂停减速预测补偿：进入暂停时按患者压力低于/不低于 30 cmH₂O 选择参数组，本次暂停保持。每 6 ms 对实际风机转速差分作 0.1 增益滤波；低压提前量 450 ms、最大 60 RPS，高压提前量 150 ms、最大 20 RPS。近端流量相对患者泄漏目标的差值从 20 降至 5 L/min 时线性启用，差值不低于 20 时不补偿；只补偿减速，稳定后自然消退。补偿位于流量 PID 积分之外，保持原有 40 RPS/周期指令限速、压力对应转速上限和抗饱和。每次进入暂停重建转速历史，转速为零、非有限或超出 800 时禁用并清除历史；恢复后的首点只建基线。供气阶段不启用，`flow_effort` 仍记录原 PID 输出。

2026-09-15 VAC 暂停 50% 模拟肺实测：PEEP 5、VT 500 mL、Ti 2000 ms、15/min，排除两口启动呼吸，各比较后续 8 口。原固件平均负向峰值 -8.120 L/min，最终固件两次独立运行分别为 -2.825 / -3.350，幅值减小 65.2% / 58.7%；整个暂停的反向气量均值 15.477→9.511 / 9.818 mL。暂停末 400 ms 平均流量峰峰值 4.303→3.605 / 4.095 L/min，平均压力 17.449→17.093 / 16.998 cmH₂O，平均 VTI 498.938→500.837 / 502.887 mL。结果为回冲明显减小，未完全消除；无独立流量分析仪复核，也未覆盖全部阻力、顺应性及泄漏。

最终版本 RTT 路径为 `build/vac_pause_scoped_final/`（PEEP 5/10/15、500 mL，各 8 口）和 `build/vac_pause_scoped_repeat_verified/`（PEEP 5 独立复测 8 口）；默认对照为 `build/vac_pause_verified_before/`，高 PEEP 原版对照为 `build/vac_pause_high_before/`。汇总及波形图为 `build/vac_pause_comparison.json` / `.png`，重算脚本 `build/vac_pause_compare.py`。最终两组 metadata 的源码和固件 SHA-256 与当前文件一致。`vac_pause_final_matrix` 是尚未限制高压入口的实验版九组采集，`vac_pause_entry_verified` 是已放弃的过早关闭实验，不代表最终固件。

PEEP 10、500 mL 的原版/最终版平均负峰为 -7.775 / -3.450 L/min，末段峰峰值 4.495 / 4.430；该对照分别为 4 / 8 口。PEEP 15、500 mL 的高压入口保持旧控制，实测负峰 -10.145 / -10.127，VTI 382.475 / 397.637 mL（设定 500），两版均存在容量不足，未在本次修复范围内解决。高 PEEP 和大潮气量的原有波动及容量不足不能标记为通过。主机新旧控制器对照覆盖供气阶段、暂停入口压力 30/40/50/60 及之后降至 30 以下，共 60 口、20,400 个控制周期，输出和诊断逐字节一致，见 `build/vac_pause_equivalence.json` / `.py`。

最终 Device Tool 构建、Flash 内容校验与 RTT 完成，`test_flow_pause.py`、`test_monitor_leak.py`、`test_vti_compensation.py` 及结构检查通过。结束时 VAC 参数为 PEEP 5、500 mL、暂停 50%、Ti 2000 ms、15/min，最后一次 RTT 已收到 `stop status=1`，当前停止通气。

VAC 高负载供气前馈：保留原 `PEEP + 参考容量/30 + f(目标流量)` 模型，并取它与 `滤波患者压力 + 上升提前量 + f(目标流量) - f(实测近端流量)` 的较大值，`f(q)=0.1572q+0.004013q²`。扣除已包含的实测流阻压降，避免重复叠加。提前量为 `clamp(2.5×(实测压力−滤波压力),0,6)` cmH₂O，沿用 0.2 压力滤波，匀速变化时约对应 60 ms 预测。仅实测流量在 0..120 L/min 内使用该负载估计；原压力和风机上限保留。

Monitor 分别记录硬限制 `volumeLimited` 和风机输出饱和 `volumeBlowerLimited`，两者都继续发布 `BREATH_RESULT_VOLUME_LIMITED`。仅时间补偿构建允许在单独的转速饱和下学习；流量补偿构建、压力受限、无效传感器、非时间切换仍禁止对应学习。每口重置两个状态，已有每口 20 ms 步长、总供气时间补偿上限和最短呼气时间保护保持；暂停时间和整个强制周期不变。`test_vti_compensation.py` 用生产 Monitor/Scheduler/Phase 验证转速饱和可进行时间学习、压力限制优先、流量补偿隔离和下一口状态重置。
