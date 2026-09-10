# User 层

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
| `app/ventlogic/` | Scheduler 为 PAC/VAC/PSV/PSV-ST 生成逐次 `stBreathPlan`，Phase Controller 执行计划，Trigger Engine 检测患者触发，Cycle Engine 完成 PSV 流量切换，Apnea Engine 调度 PSV-ST 备份呼吸，Monitor Engine 发布逐次 `stBreathResult`，Actuator Controller 统一仲裁并写入 BSP |
| `app/physalarm/` | AlarmTask 调度生理报警检测器并发布状态；PEEP 高低报警在新吸气阶段按上一周期动态 PEEP 判断，恢复条件持续 200 ms 后解除 |
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

PAC 平台期在近端流量降至 10 L/min 以下、患者压力进入目标 ±2 cmH₂O 后，本次吸气锁存低增益稳压：外环 Kp=0.25，内环 Kp=0.001、Ki=0.02。剩余流量前馈清零并通过 `pidTrackOutput` 转入内环输出；衔接实际风机转速，相对上一指令最多修正 40，反馈为零、非有限或超出 800 时回退上一指令。该处理用于减少填充结束时继续降速及随后的反复修正，不锁死风机；压力闭环和平台超压泄压继续工作。内环积分在叠加前馈后的执行器饱和时撤回同方向增量。下一次吸气恢复原增益并清除锁存和积分；PSV/ST 不进入此分支。`develop/test_flow_pause.py` 验证切换、稳压方向、模式隔离与重置；`develop/test_pac_rtt.py` 通过 Device Tool 采集默认 PAC 的台架末段波形。

2026-09-08 默认 PAC 无额外漏口模拟肺对比：PEEP 5、Delta-P 25 cmH₂O、Ti 1350 ms、20/min、氧浓度 21%、触发关闭。均排除前两次启动呼吸，使用参考下降前留出 12 ms 的末段 240 ms；按相同的随后十次呼吸比较，原版平均流量峰峰值 11.49 L/min，两次独立启动的最终固件分别为 5.622 和 5.168 L/min，降低 51.1% 和 55.0%。最终两组各分析 20 次呼吸，全组平均峰峰值分别为 4.930 和 4.561 L/min，最大单次为 9.08 和 7.50 L/min；末段压力范围分别为 29.33..30.69 和 29.15..30.57 cmH₂O。结果表明末段摆动减小，未完全消除；此窗口不包含此前填充结束的全部流量下降过程。数据为设备自身传感器测量，未覆盖其他顺应性、漏气或参数组合。原始数据位于 `build/pac_baseline/`、`build/pac_final_run1/`、`build/pac_final_run2/`，统一窗口统计为 `build/pac_comparison.json`；均经 Device Tool 编译/烧录/RTT 入口，测试结束已收到停止确认。

当前 GD32F470 板载 HXTAL 为 8 MHz，系统使用 `240M_PLL_8M_HXTAL` 配置；该配置决定 RTOS tick 和 APB 外设（包括 VCM UART 230400）的实际时基。

PEEP 报警以当前呼吸计划的 `peepCmh2o` 为 refpeep。在新吸气阶段，`PHYS_ALARM_PEEP_HIGH` 按上一周期 `MONITOR_DYN_PEEP > refpeep + 5` 触发，`PHYS_ALARM_PEEP_LOW` 按 `< refpeep - 3` 触发，每个吸气计划只判断一次。AlarmTask 每 10 ms 检查恢复：动态 PEEP 严格低于高限或严格高于低限持续至少 200 ms 后解除对应报警；等于阈值会中断恢复计时。恢复不使用实时患者压力。无上一完整周期时不触发，停止通气或进入零点补偿阶段时清除 PEEP 报警和计时。检测器仅由 AlarmTask 调用，通过 RTOS 临界区读取 VentTask 的计划、阶段和监测快照，不用于 ISR。

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

VAC 吸气暂停继续使用近端流量反馈，不锁定患者压力。入口采用 Kp=0.003、Kd=0.00005、Ki=0 制动供气尾段；至少经过 120 ms，且近端流量下降到泄漏目标上方 2 L/min 以内后，开启 Ki=0.02 的积分补偿。此时按患者压力一次性选择稳定段参数：低于 30 cmH₂O 保留 Kp=0.003、Kd=0.00005；达到或超过 30 cmH₂O 使用 Kp=0.0003、Kd=0，减少高压力工况的反馈振荡。本次暂停内不反复切换增益。暂停风机指令每 6 ms 最多变化 40 个指令单位，绝对压力对应转速上限优先于变化率限制，限幅时撤回同方向积分增量。患者侧漏气才计入近端流量目标，呼气阀侧流量不能直接当作患者侧漏气目标。验证重点是暂停稳定后 patflow 相对泄漏补偿目标的偏差与振幅，不以原始过零次数作为验收标准。

`PHYS_ALARM_CPAP_TOO_HIGH` 已启用：以当前呼吸计划 `peepCmh2o` 为基准，`INSP_REAL_PRS` 与 `PAT_REAL_PRS` 同时严格大于 PEEP + 15 cmH₂O 持续 15 s 触发；报警后两路同时严格小于 PEEP + 14.5 cmH₂O 持续 3 s 恢复。等于阈值或任一路不满足条件会中断对应计时，吸呼气切换不清计时。停机、零点补偿阶段清除状态；计划不可用时中断计时并保留报警状态。由 AlarmTask 在临界区获取计划及两路压力快照。`develop/test_monitor_leak.py` 包含阈值、计时边界、中断、阶段切换及 tick 回绕回归。

`stVentPatientSettings.useHostSettings` 默认 0，使用本机固定设置；1 使用独立的 MCM 设置副本。两种情况下均接收并缓存参数；MCM 报警限（0xAC）不受此开关限制，在 VentTask 中按 scale 解码并写入当前报警设置，未下发的字段保留原值，切换参数源时重新应用已缓存的报警限。压力和呼出潮气量检测器直接读取这些设置；窒息时间同步至 PSV/PSV-ST 设置。通气命令 1 启动、0 停止。已绑定 PAC/VAC/CPAP-PSV/PSV-ST 的现有字段，其他模式仅缓存，由 Scheduler 拒绝启动。时间按 scale 解码为秒后转为毫秒。波形沿用压力 ×10、流量 ×10+2000、容量 mL 和毫秒时间戳；相位 1 吸气、2 呼气。CommTask 优先级 5、周期 10 ms、栈 1024 words，低于通气和传感器任务。

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

`MONITOR_HMI_RES_INSP = 60 × (Ppeak - PEEP) / Qinsppeak`，`MONITOR_HMI_RES_EXP = 60 × (Pplat - PEEP) / Qexppeak`，流量使用近端 `PAT_REAL_FLOW`（L/min），呼气峰值取呼气阶段负流量最大绝对值；PEEP 沿用完成结果的呼气末患者压力。两项随完整呼吸结算并保持到下次发布，吸气和呼气公式均乘 60，将 L/min 换算为 L/s。非有限采样、缺少对应压力、零峰值或负/非有限计算结果时置零且不上传；停止、调零时清零。CommTask 从同一 `stBreathResult` 快照上传 MCM `0x16` / `0x17`，沿用无符号 16 位、scale=0 的协议定义（截断小数、限幅 0..65535）及队列重试。主机回归覆盖公式、峰值相位隔离、跨周期清零、无平台压、异常采样和报文编码。

`MONITOR_HMI_C_DYNC = VTi / (Ppeak - PEEP)`，`MONITOR_HMI_C_STAT = VTe / (Pplat - PEEP)`，单位 mL/cmH₂O；使用同一完整呼吸的容量、压力和呼气末 PEEP，通过 `stBreathResult.complianceDynamic` / `complianceStatic` 同步发布并保持到下次结算。压差非正或非有限、容量为负、缺少对应有效压力/容量或周期采样异常时置零且不上传；停止、调零时清零。MCM 静态顺应性为 `0x18`，动态顺应性为 `0x19`，均使用无符号 16 位、数值 ×10、scale=1，沿用逐呼吸上传和队列重试；主机回归覆盖公式、无平台压、零/负压差、异常采样、快照保持、清零和报文缩放。

近端流量校准参考为开放管路下 SFM3119 未乘 BTPS 的原始 SLM（20°C、1013 hPa），不是运行期患者侧实际体积流量。保留既有 14 Hz ADC 滤波；`PAT_REAL_FLOW` 查表前先减运行期 ADC 零漂，再围绕表内反插值的真实零流量 ADC 乘密度比 `1 + PAT_REAL_PRS / CALIBTRANS_AMBIENT_PRESSURE_CMH2O`。患者压力来自同轮 Butterworth 压力换算。密度比在差压域应用后再查原表，正负方向均使用自身曲线，不使用固定 VTe 比例。此模型假定 ADC 与差压线性、校准与运行温度/气体黏度近似相同、开放标定压力接近大气；环境压力暂按 1013 hPa，无气压或近端温度实测补偿。

调零接口继续接收“当前累计值 + 实测残差”，通过反查同一校准表将残差换成 ADC 偏移；getter 是累计调零标记，不能作为恒定流量误差扣除。气体模式切换不改变 ADC 偏移。校准不可用或换算非有限时发布 NaN，供监测拒绝该周期。EEPROM 表及潮气量积分定义不变。`develop/test_flow_conversion.py` 使用真实换算/数据处理源码，覆盖非对称非线性表、双向零漂、重复调零、SFM3119/BTPS 基准、压力密度补偿及异常恢复。

启动调零需要连续 1000 ms 满足入口流量及患者压力的独立静息门限，并要求近端流量样本为有限值；任一条件中断即重新计时，3000 ms 超时保留旧零点。近端流量的绝对值不再作为准入门限，避免超过旧 ±10 L/min 门限的零漂反向锁死补偿。补偿写入失败会中止本次启动并记录错误，不再误报成功。此窗口用于降低滤波后相关零点噪声对整周期容量的偏置，不根据 VTi/VTe 差值自动学习零点。`test_flow_zero_offset.py` 验证大零漂、写入失败、完整窗口前不更新以及超时保留。

2026-09-08 潮气量换算台架回归：模拟肺 PAC，PEEP 5、Delta-P 25 cmH₂O、Ti 1350 ms、20/min、21% 氧、触发关闭。统一排除前两次启动呼吸并取随后 13 次：旧版平均 VTe−VTi 为 22.80 mL（3.34%）；最终 1000 ms 调零固件两次分别为 12.69 mL（1.78%）和 20.06 mL（2.84%），最大单次差值分别为 21.76、28.45 mL，未消除误差。原始日志/结果在 `build/volume_balance_old/`、`build/volume_balance_zero_long/`、`build/volume_balance_zero_repeat/`；统一统计、固件及源码 SHA-256 在 `build/volume_balance_comparison.json`。VAC 500 mL、PEEP 5、Ti 2000 ms、15/min、无暂停的末五次 VTI 为 495.93..503.00 mL，通过原 ±35 mL 目标验收，记录在 `build/volume_balance_vac/`。其中末次 VTi/VTe 为 500.02/533.09 mL，同时 PEEP 降到 3.48 cmH₂O，提示周期首尾储气变化仍影响两者比较；不能把容量目标通过解读为每次吸呼气相等。全部经 Device Tool 构建/烧录/RTT，最终收到停机确认；仅设备自身测量，无独立呼气参考仪器。
