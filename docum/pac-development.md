# PAC 功能开发建议

本文基于 2026-08-26 工作区中的实际代码，说明 PAC（Pressure Assist/Control，压力辅助/控制）模式下一步应如何实现。本文是开发和台架联调建议，不替代产品需求、风险管理、临床参数定义或法规验证，也不能作为患者使用放行依据。

## 1. 当前结论

PAC 的计划、相位和执行器主链已经形成：

```text
settingdata
    -> breathScheduler（参数校验并逐次生成 stBreathPlan）
    -> phaseController（执行 BreathPlan 并生成参考轨迹）
    -> pressureController / expirationController
    -> stActuatorRequest
    -> actuatorController（按字段 ownership 仲裁）
    -> blower / expiratory valve / oxygen valve

monitorEngine
    -> stBreathResult（VTi、VTe、Ppeak、PEEP、触发原因和周期）
```

现阶段继续沿用这个分层，不增加新的 manager。`breathScheduler` 决定下一次 breath，`phaseController` 只执行计划，`ventalgo` 下的控制器只生成统一请求，`actuatorController` 是唯一的执行器写入者。当前已接入 PAC 压力/流量患者触发检测；快速安全监督和独立报警闭环仍未完成，因此不能用于患者。

## 2. 现状检查

| 项目 | 当前状态 | 影响 |
| --- | --- | --- |
| PAC 参数结构 | `stVentPacSettings` 已存在，并有默认值 | 可以继续使用，但字段命名和单位不统一，需要先固定 contract |
| 参数校验 | 已校验主要有限值、压力、频率、吸气时间、上升时间、暂停比例和触发枚举 | 运行期设置源仍是全局可写对象，尚无 pending/active 原子提交语义 |
| 启停 API | `venttest` 已接入 PAC/VAC start/stop | 仅供台架调试，不能作为产品控制入口 |
| 呼吸时序 | Scheduler 逐次产生 `stBreathPlan`，Phase Controller 用 32 位单调 tick 执行 Rise/Hold/Release/PEEP | PAC 已支持 PEEP 期压力/流量触发，阈值和滤波参数仍需台架整定 |
| 压力闭环 | `pressureController` 实现吸气压力级联控制 | 增益和前馈仍需肺模型台架确认 |
| 公共呼气控制 | `expirationController` 统一实现 Release 和 PEEP | 安全阀位和故障策略仍需风险分析确认 |
| 执行器层 | 压力、流量和呼气控制器统一生成 `stActuatorRequest`，只有 `actuatorController` 写 BSP | 尚未接入高压、传感器和风机故障的最高优先级覆盖 |
| 监测与报警 | `monitorEngine` 已发布逐次 `stBreathResult`；`AlarmTask` 仍为空 | 已有结果 contract，但快速安全联锁和报警锁存尚未实现 |
| 构建配置 | 新增 `expirationcontroller.c` 并纳入 CMake | Device Tool Build 已通过 |
| 调度周期 | `SensorTask` 为 3 ms，`VentTask` 为 6 ms | 控制器固定采样周期为 0.006 s |

## 3. 先固定 PAC 数据 contract

当前迭代可暂不重命名 `stVentPacSettings` 字段，避免把重构和功能实现混在一起，但代码和接口必须按下表解释：

| 字段 | 单位 | PAC 含义 | 最低校验要求 |
| --- | --- | --- | --- |
| `oxygen` | % | 目标吸入氧浓度 | 有限值，位于氧浓度上下限内 |
| `peep` | cmH2O | 呼气末目标压力 | 有限值，位于压力范围内 |
| `Rate` | 次/min | 强制呼吸频率 | 有限且大于 0，位于频率范围内 |
| `inspiratoryTimeMs` | ms | 每次吸气总时间 | 大于 0，且小于呼吸周期减去最小呼气时间 |
| `DeltaPressure` | cmH2O | PEEP 之上的驱动压力 | 有限且不小于 0，`peep + DeltaPressure` 不超过高压限制 |
| `riseTimeMs` | ms | 从当前压力上升到吸气压力的时间 | 不大于 `inspiratoryTimeMs` |
| `triggerType` | 枚举 | 关闭、压力触发或流量触发 | 必须小于 `VENT_TRIGGER_COUNT` |
| `pressureTriggerCmh2o` | cmH2O | 相对稳定基线的压力下降阈值 | 压力触发时必须为有限非零值；当前检测取其绝对值，推荐设置保存为负值 |
| `flowTriggerLpm` | L/min | 患者吸气流量阈值 | 流量触发时必须为有限正值，允许范围由产品需求固定 |

建议尽快把设置访问从“返回全局可写指针”改为“复制快照 + 校验 + 一次提交”。HMI 或 console 只写 pending 设置，VentTask 在一个明确的周期边界提交 active 设置。这样 `Rate`、吸气时间和压力不会在同一个呼吸周期内读到不同版本。临界区只能通过 `rtos.h` 中的项目接口实现。

## 4. 推荐的模块职责

| 模块 | 应负责 | 不应负责 |
| --- | --- | --- |
| `settingdata.*` | 设置快照、默认值、读写和版本号 | 呼吸状态机、硬件输出 |
| `breathscheduler.*` | 模式校验、启停、决定下一次 breath 并生成不可变 `stBreathPlan` | PID、直接写阀门或风机 |
| `phasecontroller.*` | 执行 `stBreathPlan`、呼吸相位切换、参考轨迹和触发入口 | 判断 PAC/VAC 模式、读取 HMI 可变设置、直接写硬件 |
| `triggerengine.*` | 压力/流量基线、去抖、锁定期和单次触发事件 | 改模式参数、直接开始执行器输出 |
| `pressurecontroller.*` | 吸气参考压力与实测压力之间的闭环，生成 `stActuatorRequest` | Release、PEEP、FiO2 或呼吸相位 |
| `expirationcontroller.*` | 所有普通 breath 共用的 Release 和 PEEP 请求 | 判断当前通气模式或直接写 BSP |
| `fio2controller.*` | 根据空气流量和氧气流量形成混氧请求 | 改变呼吸时序 |
| `actuatorcontroller.*` | 请求仲裁、限幅、斜率限制、唯一写入 BSP、安全态 | 保存模式设置或复制状态机 |
| `monitorengine.*` | 连续监测并在呼吸边界发布不可变 `stBreathResult` | 直接改变正常闭环输出 |
| `AlarmTask` 或安全监督模块 | 独立检查高压、传感器、风机连接和超时，触发安全停机 | 代替正常模式控制器 |

关键原则是执行器单一所有权：`pressureController`、`flowController`、`expirationController` 和 `fio2Controller` 只产生带字段有效位的请求，只有 `actuatorController` 能调用 `blowerVcmSendControl()` 和 `dvalveDutySet()`。当前已经按 phase 和 breath type 选择唯一的主要控制器，未实现的 FiO2 输出不会声明氧阀 ownership。

## 5. 推荐实现顺序

### 阶段 0：可构建基线（已完成）

| 修改位置 | 建议 | 完成标准 |
| --- | --- | --- |
| `CMakeLists.txt` | 保留 `actuatorController` 作为聚合入口并加入公共 expiration controller | Device Tool Build 成功 |
| `User/user.md` | 同步 BreathPlan、ActuatorRequest、BreathResult 和 3 ms/6 ms 周期 | 文档与实现一致 |
| 启动入口 | 先增加仅供台架使用的 PAC start/stop 命令或 HMI 调用 | 能观察到 start/stop 返回码，非法设置不能启动 |

不要在 `main()` 中用默认参数自动启动通气。上电应保持安全 idle，只有设置校验成功且收到明确启动命令后才进入 PAC。

### 阶段 1：把时序闭环单独跑通

先不连接真实执行器，只记录以下信息到可裁剪 RTT 日志或监测数据：相位、周期序号、压力参考值、实际压力、触发原因。日志必须使用 `LOG_I/LOG_W/LOG_E`，不能使用标准库输出。

建议补充这些最小能力：

- `phaseControllerGetState()`：让执行器层按相位选择控制策略。
- `phaseControllerStop()`：立即进入 `PHASE_IDLE`，清零计数并把参考压力切到定义好的安全值。
- 周期开始事件：用于 PID reset、峰压/潮气量统计清零和设置快照切换。
- 所有时间比较使用明确的毫秒整数或 32 位累计量，避免长期运行中的窄类型溢出和浮点到整数的隐式边界问题。

PAC 的基本状态流建议为：

```text
IDLE
  -> EXP_RELEASE/预备态
  -> EXP_PEEP
  -> [时间到 或 合法患者触发]
  -> INSP_RISE
  -> INSP_HOLD
  -> EXP_RELEASE
```

患者触发只能缩短等待吸气的时间，不能绕过最小呼气锁定期；一次触发只产生一个事件，进入吸气后立即锁定，避免噪声重复触发。具体触发窗口和阈值必须来自产品需求，不能由开发者临时猜测。

### 阶段 2：实现压力闭环，不先做自动整定

复用 `User/tools/controller/pid.*`，按 VentTask 实际周期把 `samplePeriod` 设置为 `0.006F`。建议先使用 PI，将 `kd` 保持为 0；初始增益必须通过肺模型台架辨识和逐级调试得到，不应写入文档猜测值。

压力控制器每周期执行以下固定步骤：

1. 从 `phaseControlGet(PHASE_REF_PRESSURE)` 取得参考压力。
2. 从 `controlDataGet(INSP_REAL_PRS)` 取得经过校准的实测压力。
3. 校验测量有效性、校准状态和有限值；无效时返回错误并请求安全态。
4. 用 PID 计算归一化控制请求，并执行输出限幅和变化率限制。
5. 把请求交给 `actuatorController`，不能直接写风机或阀门。
6. 在 start、stop、相位切换和故障恢复时明确调用 `pidReset()`，避免积分残留造成压力冲击。

第一版建议只选择一个主要操纵量完成吸气压力闭环，例如风机目标；呼气阀在吸气期保持经过台架确认的状态。不要一开始同时闭环调风机和呼气阀，否则难以区分两条控制路径的作用。

### 阶段 3：实现呼气释放和 PEEP

全局相位在整个呼气期保持 `PHASE_EXP`。`expirationController` 在内部依次处理 `RELEASE`、`CAPTURE` 和 `PEEP`：快速卸压、捕获目标压力，再维持 PEEP；输出请求仍由 `actuatorController` 统一提交。

需要明确以下行为：

- 从吸气切到呼气时，吸气 PID 是否 reset，风机如何降速，呼气阀如何打开。
- 到达 PEEP 后，使用固定前馈还是 PI 微调。
- 风机命令与呼气阀命令的限幅、变化率和故障默认值。
- 停机时风机、氧阀、泄压阀、呼气阀各自的安全状态。

这些安全状态必须由硬件和产品风险分析确认，不能仅凭软件命名推断阀门通断含义。

### 阶段 4：接入患者触发（第一版已完成）

`triggerengine` 应在 `PHASE_EXP` 且 `expirationController` 已完成捕获后工作，输入优先使用已校准且专门滤波的数据：

| 触发类型 | 建议输入 | 必需保护 |
| --- | --- | --- |
| 压力触发 | 患者侧压力相对呼气稳定基线的下降量 | 最小呼气锁定、基线更新冻结、连续样本确认、触发后锁定 |
| 流量触发 | `MDIFF_REAL_FLOW`；如该通道不满足触发带宽，再评估专用滤波链 | 零漂补偿、方向确认、连续样本确认、泄漏抑制、触发后锁定 |

当前 `INSP_FLOW_TRIGER_FILTERED` 仍是未校准的 SFM 吸气流量，不能在没有单位和方向验证的情况下直接当作患者触发量。触发成功后应向 `phaseController` 发送事件，不要由 `triggerengine` 自己修改全局相位对象。

当前实现仅在 PAC 的 `PHASE_EXP` 且呼气捕获完成后建立基线和检测事件：进入稳定 PEEP 后先用 8 个 VentTask 样本（约 48 ms）稳定基线，压力下降或近端吸气流量增量达到阈值并连续保持 3 个样本（约 18 ms）后，调用 `phaseControllerTrigger()`。候选成立期间冻结基线；相位控制器继续负责最小呼气锁定和触发类型校验。成功进入吸气后立即清除候选，因此一次努力只产生一次触发。

RTT 台架命令如下，阈值参数使用百分之一单位，配置命令会先停止通气：

```text
vt trigger pressure 200   # 压力下降 2.00 cmH2O
vt trigger flow 300       # 流量增加 3.00 L/min
vt trigger off
vt pac
vt status                 # BREATH_RESULT 中 trigger=2 为压力，trigger=3 为流量
```

8/3 个样本和基线增益 `0.10` 是第一版工程参数，不是临床定值。上板前必须用肺模型确认近端流量正方向、泄漏下的基线稳定性、噪声误触发率和锁定期行为。

### 阶段 5：FiO2、监测和报警

FiO2 控制建议最后接入。现有数据包含空气流量和氧气流量，可先做受限的前馈配比，再根据产品是否具备独立氧浓度传感器决定能否形成真正的 FiO2 闭环。仅靠两路流量不能替代氧浓度监测和相关报警。

PAC 上台架前至少需要这些独立保护：

- 压力超过已提交的高压限制时立即撤销正常控制请求并进入经确认的泄压状态。
- 压力传感器校准无效、数值非有限或长期不更新时停止闭环。
- 风机反馈超时、控制发送持续失败或转速明显异常时停止通气并报告故障。
- 设置更新失败时保留上一份完整 active 设置，不能提交半套参数。
- VentTask 超周期、状态机卡死或吸气时间超限时进入安全态。
- stop 和故障停机必须幂等，多次调用结果一致。

高压保护不应只依赖 10 ms 的 AlarmTask；VentTask 内需要快速联锁，AlarmTask 负责独立复核、锁存和上报。

### PSV/PSV-ST 第一版扩展

当前 Scheduler 也支持 `VENT_MD_CPAP_PSV` 和 `VENT_MD_PSV_ST`。PSV 计划使用 `BREATH_TYPE_SPONTANEOUS_PRESSURE_SUPPORT`，患者触发后复用压力控制器；Cycle Engine 在吸气期记录近端峰值流量，经过最小吸气锁定后，在流量下降至 `cycleOffPercent` 且连续确认 3 个样本时进入呼气。没有达到流量切换条件时，由 `maximumInspiratoryTimeMs` 强制结束吸气。

`stBreathPlan` 已增加 Cycle 类型、Cycle 百分比、最小/最大吸气时间、窒息时间、备份呼吸周期和时间触发开关。PAC/VAC 仍使用时间切换和时间触发；PSV 使用患者触发和流量切换。

Apnea Engine 在 CPAP/PSV 中超时后进入 `APNEA_ENGINE_ALARM`，等待后续 Alarm Engine 消费；在 PSV-ST 中则请求 Scheduler 产生 `BREATH_TRIGGER_REASON_APNEA_BACKUP` 的强制压力呼吸，并按 backup rate 重复。任何新的压力或流量患者触发都会退出 backup，恢复 PSV。

RTT 台架入口：

```text
vt psv                  # 启动 CPAP/PSV 默认设置
vt psvst                # 启动 PSV-ST 默认设置
vt trigger pressure 200 # 修改当前已选择模式为 2.00 cmH2O 压力触发；命令会停止通气
vt trigger flow 300     # 修改当前已选择模式为 3.00 L/min 流量触发；命令会停止通气
vt status               # 查看 trigger、cycle_reason、Ti、峰值吸气流量和 apnea state
```

第一版 Cycle 参数为最小吸气时间 200 ms、连续确认 3 个 VentTask 样本；这些仅是工程默认值。当前尚未实现漏气补偿、自动触发抑制、双触发统计和正式窒息报警锁存，因此仍需肺模型台架调参和安全监督后才能进入产品验证。

## 6. 建议的数据流和调用顺序

VentTask 每 6 ms 的建议顺序如下：

```text
更新滤波/校准数据
    -> 快速安全检查
    -> scheduler 处理设置提交与启停命令
    -> phaseController 先处理时间边界并更新相位
    -> triggerEngine 在仍处于 PEEP 时产生一次性触发事件
    -> 按当前相位运行 pressure 或 PEEP 控制器
    -> FiO2 控制器产生混氧请求
    -> actuatorController 仲裁、限幅并一次性提交硬件输出
    -> monitorEngine 更新本周期监测值
```

SensorTask 继续负责传感器和风机通信处理。跨任务只交换小型快照、状态码或版本号，不让 VentTask 和 SensorTask 同时写同一个执行器状态对象。

## 7. 验证矩阵

| 层级 | 测试内容 | 通过标准 |
| --- | --- | --- |
| 参数单元测试 | 频率为 0、NaN/Inf、压力越界、吸气时间过长、非法触发类型 | 全部拒绝，active 设置不改变 |
| 状态机测试 | 不同 Rate、Ti、rise time；tick 边界；start/stop；重复 stop | 相位顺序、周期和参考轨迹符合设置，误差不超过一个 VentTask 周期 |
| 触发测试 | 阈值上下噪声、持续泄漏、锁定期内触发、单次有效努力 | 无重复触发，锁定期不触发，有效事件只缩短一次呼气等待 |
| 控制器测试 | 设定值阶跃、测量无效、输出饱和、相位切换 | 输出有限且受限，无积分残留，故障进入安全请求 |
| 执行器测试 | 风机 TX busy/掉线、阀门限幅、连续 stop | 无非法命令，故障有日志和锁存状态，安全态可重复进入 |
| 肺模型台架 | 顺应性和阻力组合、泄漏、气道阻塞、传感器断线 | 压力不超过限制，时序正确，所有故障均能检测并进入确认过的安全态 |
| 长时间测试 | tick 回绕、数小时连续运行、频繁参数更新 | 无状态卡死、计数溢出、输出漂移或数据撕裂 |

每完成一个参与编译的阶段，都应通过 VS Code 的 `Device Tool: Build`；需要上板时依次使用 `Device Tool: Flash`、`Reset` 和 `RTT`。当前 Windows 仓库对应入口为 `py -3 user/develop/quick_deploy.py <action>`，不要绕过 Device Tool 工作流。

## 8. 最小可交付里程碑

建议把“PAC 第一版完成”限定为以下范围：

- 仅支持 `VENT_MD_PAC`，其他模式明确返回 `BREATH_CONTROL_ERROR_UNSUPPORTED`。
- 上电 idle，合法设置可 start，任意时刻可 stop。
- 强制时间呼吸、压力斜坡、吸气保持、呼气释放和 PEEP 状态完整运行。
- 压力闭环只有一个主要操纵量，执行器只有一个写入所有者。
- 压力或流量患者触发至少完成一种，另一种保持明确不支持，不静默退化。
- 高压、压力测量无效和风机掉线均能触发经台架确认的安全停机。
- RTT 能观察设置版本、相位、触发原因、参考压力、实测压力、控制输出和故障码。
- 参数、状态机和控制器有主机侧测试，真实硬件完成肺模型台架验证。

达到这些条件后，再扩展 FiO2 闭环、泄漏补偿、第二种患者触发和其他通气模式，可以避免 PAC 尚未闭环时同时铺开过多功能。
