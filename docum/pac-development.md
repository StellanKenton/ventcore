# PAC 功能开发建议

本文基于 2026-08-24 工作区中的实际代码，说明 PAC（Pressure Assist/Control，压力辅助/控制）模式下一步应如何实现。本文是开发和台架联调建议，不替代产品需求、风险管理、临床参数定义或法规验证，也不能作为患者使用放行依据。

## 1. 当前结论

PAC 主链路已经搭出框架，但尚未形成可运行的闭环：

```text
settingdata
    -> breathScheduler（参数校验与装载）
    -> phaseController（生成压力参考值）
    -> pressureController（当前为空）
    -> actuatorController（当前只轮询四个空控制器）
    -> blower / expiratory valve / oxygen valve
```

现阶段可以继续沿用这个分层，不建议再增加新的 manager。`breathScheduler` 负责模式和参数，`phaseController` 负责时序和参考轨迹，`ventalgo` 下的控制器负责计算控制请求，`actuatorController` 是唯一的执行器写入者。

## 2. 现状检查

| 项目 | 当前状态 | 影响 |
| --- | --- | --- |
| PAC 参数结构 | `stVentPacSettings` 已存在，并有默认值 | 可以继续使用，但字段命名和单位不统一，需要先固定 contract |
| 参数校验 | 已校验压力、频率、吸气时间、上升时间和部分患者信息 | 未校验触发阈值的有限性与范围，运行期更新也没有原子提交语义 |
| 启停 API | 已有 `breathSchedulerStart/Stop/SettingsUpdate` | 仓库中没有调用者，固件启动后不会真正开始 PAC |
| 呼吸时序 | 已有 `IDLE/Rise/Hold/Release/PEEP` 状态和压力斜坡 | 只支持时间触发；患者触发参数尚未接入，停机后参考值和执行器安全态未定义 |
| 压力闭环 | `pressurecontroller.c` 为空 | 压力参考值不能转化为风机或阀门控制量 |
| 执行器层 | 风机、氧阀、泄压阀、呼气阀 BSP 已有接口 | 业务控制器尚未调用；四个控制器若各自写硬件会产生所有权冲突 |
| 监测与报警 | `monitorengine` 和 `AlarmTask` 为空 | 高压、传感器失效、风机掉线等没有形成联锁 |
| 构建配置 | `ventalgo.c/.h` 已删除，但 `CMakeLists.txt` 仍列出 `ventalgo.c` | 当前 CMake 构建会因源文件不存在而失败，必须先修正 |
| 调度周期 | `SensorTask` 为 3 ms，`VentTask` 为 6 ms | PID 采样周期应使用 0.006 s；现有 `User/user.md` 中的 2 ms 和 10 Hz 描述已过期 |

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
| `pressureTriggerCmh2o` | cmH2O | 相对稳定基线的压力下降阈值 | 压力触发时必须为有限值，方向和允许范围由产品需求固定 |
| `flowTriggerLpm` | L/min | 患者吸气流量阈值 | 流量触发时必须为有限正值，允许范围由产品需求固定 |

建议尽快把设置访问从“返回全局可写指针”改为“复制快照 + 校验 + 一次提交”。HMI 或 console 只写 pending 设置，VentTask 在一个明确的周期边界提交 active 设置。这样 `Rate`、吸气时间和压力不会在同一个呼吸周期内读到不同版本。临界区只能通过 `rtos.h` 中的项目接口实现。

## 4. 推荐的模块职责

| 模块 | 应负责 | 不应负责 |
| --- | --- | --- |
| `settingdata.*` | 设置快照、默认值、读写和版本号 | 呼吸状态机、硬件输出 |
| `breathscheduler.*` | 模式校验、启停、将 PAC 设置转换为本周期参数 | PID、直接写阀门或风机 |
| `phasecontroller.*` | 呼吸相位切换、压力参考轨迹、相位查询 | 读取 HMI 可变设置、直接写硬件 |
| `triggerengine.*` | 压力/流量基线、去抖、锁定期和单次触发事件 | 改模式参数、直接开始执行器输出 |
| `pressurecontroller.*` | 参考压力与实测压力之间的闭环计算 | 同时控制 FiO2 或决定呼吸相位 |
| `peepcontroller.*` | 呼气阶段的 PEEP 控制请求 | 与压力控制器同时写同一个硬件 |
| `fio2controller.*` | 根据空气流量和氧气流量形成混氧请求 | 改变呼吸时序 |
| `actuatorcontroller.*` | 请求仲裁、限幅、斜率限制、唯一写入 BSP、安全态 | 保存模式设置或复制状态机 |
| `monitorengine.*` | 计算峰压、PEEP、潮气量、频率等监测结果 | 直接改变正常闭环输出 |
| `AlarmTask` 或安全监督模块 | 独立检查高压、传感器、风机连接和超时，触发安全停机 | 代替正常模式控制器 |

关键原则是执行器单一所有权：`pressureController`、`peepController` 和 `fio2Controller` 只产生请求，只有 `actuatorController` 能调用 `blowerVcmSendControl()` 和 `dvalveDutySet()`。当前四个控制器每 6 ms 无条件依次运行的结构，需要增加“按相位选择控制器 + 集中提交输出”，否则后执行的控制器可能覆盖前一个控制器。

## 5. 推荐实现顺序

### 阶段 0：恢复可构建基线

| 修改位置 | 建议 | 完成标准 |
| --- | --- | --- |
| `CMakeLists.txt` | 按当前方向移除已删除的 `User/app/ventalgo/ventalgo.c`，保留 `actuatorController` 作为聚合入口 | Device Tool Build 成功 |
| `User/user.md` | 后续同步真实任务名和 3 ms/6 ms 周期 | 文档与 `taskmanager.h` 一致 |
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

`PHASE_EXP_RELEASE` 与 `PHASE_EXP_PEEP` 应使用不同策略：前者快速卸压，后者维持 PEEP。`peepController` 可以输出呼气阀请求，但仍由 `actuatorController` 统一提交。

需要明确以下行为：

- 从吸气切到呼气时，吸气 PID 是否 reset，风机如何降速，呼气阀如何打开。
- 到达 PEEP 后，使用固定前馈还是 PI 微调。
- 风机命令与呼气阀命令的限幅、变化率和故障默认值。
- 停机时风机、氧阀、泄压阀、呼气阀各自的安全状态。

这些安全状态必须由硬件和产品风险分析确认，不能仅凭软件命名推断阀门通断含义。

### 阶段 4：接入患者触发

`triggerengine` 应在 `PHASE_EXP_PEEP` 中工作，输入优先使用已校准且专门滤波的数据：

| 触发类型 | 建议输入 | 必需保护 |
| --- | --- | --- |
| 压力触发 | 患者侧压力相对呼气稳定基线的下降量 | 最小呼气锁定、基线更新冻结、连续样本确认、触发后锁定 |
| 流量触发 | `MDIFF_REAL_FLOW`；如该通道不满足触发带宽，再评估专用滤波链 | 零漂补偿、方向确认、连续样本确认、泄漏抑制、触发后锁定 |

当前 `INSP_FLOW_TRIGER_FILTERED` 仍是未校准的 SFM 吸气流量，不能在没有单位和方向验证的情况下直接当作患者触发量。触发成功后应向 `phaseController` 发送事件，不要由 `triggerengine` 自己修改全局相位对象。

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

## 6. 建议的数据流和调用顺序

VentTask 每 6 ms 的建议顺序如下：

```text
更新滤波/校准数据
    -> 快速安全检查
    -> scheduler 处理设置提交与启停命令
    -> triggerEngine 产生一次性触发事件
    -> phaseController 更新相位和压力参考
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
