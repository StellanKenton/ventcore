# User 层

当前 User 层包含板级外设驱动、RTOS 启动、任务骨架和基于 SEGGER RTT 的日志/控制台系统。

| 路径 | 职责 |
|---|---|
| `main.c` | 初始化日志和控制台、注册 worker tasks 并启动调度器 |
| `app/calibration/` | 上电只读加载 EEPROM 中五类既有校准记录，校验记录头与 CRC-16，并提供有效性查询和运行期只读访问 |
| `app/system/taskmanager.*` | 通过 `WorkerTasksRegister()` 创建 defaultTask、VentTask、SensorTask、SysTask、AlarmTask；全部任务使用绝对周期 `DelayUntil`，SensorTask 每 3 ms 采集传感器并处理风机反馈，VentTask 每 6 ms 执行通气控制链 |
| `app/databus/` | 维护控制数据数组；SensorTask 保存当前及前一周期原始数据，VentTask 基于最新原始数据完成滤波和校准转换 |
| `app/ventalgo/` | 实现吸气压力、吸气流量、公共 Release/PEEP 和 FiO₂ 控制器；各控制器只生成统一 `stActuatorRequest`，不直接写 BSP |
| `app/ventlogic/` | Scheduler 为 PAC/VAC/PSV/PSV-ST 生成逐次 `stBreathPlan`，Phase Controller 执行计划，Trigger Engine 检测患者触发，Cycle Engine 完成 PSV 流量切换，Apnea Engine 调度 PSV-ST 备份呼吸，Monitor Engine 发布逐次 `stBreathResult`，Actuator Controller 统一仲裁并写入 BSP |
| `app/physalarm/` | AlarmTask 调度生理报警检测器并发布状态；PEEP 高低报警在新吸气阶段按上一周期动态 PEEP 判断，恢复条件持续 200 ms 后解除 |
| `bsp/adc/adc.*` | 使用 ADC1 规则组扫描、连续转换和 DMA1 循环模式持续采集 14 路板级模拟量 |
| `bsp/blower_vcm/blower_vcm.*` | 使用 UART4（板级 VCM UART5，PC12/PD2）和 DMA0 异步发送双控制帧、循环接收反馈；控制变化时立即发送并每 100 ms 保活重发，提供连接超时与通信统计 |
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

`vt volume <peep> <ml> [pause_pct]` 通过 RTT 设置 VAC 参数；暂停百分比为 0..99，省略时保留当前值，上电默认 0。`vt volume 15 500 0` 设置无暂停，供气覆盖完整吸气时间，目标流量按有效供气时间计算，`vt run 1` 启动；`vt status` 波形的 `volume_pause` 标记暂停阶段，`pause_settled` 标记已切入稳定段 PI，`leak_lpm` 记录患者侧泄漏估计。`develop/test_vac_matrix.py` 经 Device Tool RTT 入口完成模拟肺九组测试，保存原始日志、波形及逐呼吸振幅、泄漏目标误差统计；过零次数仅作辅助诊断。
| `FreeRTOSConfig.h` | FreeRTOS 工程配置 |

项目代码只能通过 `rtos.h` 使用任务、调度、tick 和临界区能力；FreeRTOS 原生 API 仅允许出现在 `portrtos.c`。日志统一使用 `LOG_I`、`LOG_W`、`LOG_E` 等宏，不能直接使用标准库输出函数。

当前 GD32F470 板载 HXTAL 为 8 MHz，系统使用 `240M_PLL_8M_HXTAL` 配置；该配置决定 RTOS tick 和 APB 外设（包括 VCM UART 230400）的实际时基。

PEEP 报警以当前呼吸计划的 `peepCmh2o` 为 refpeep。在新吸气阶段，`PHYS_ALARM_PEEP_HIGH` 按上一周期 `MONITOR_DYN_PEEP > refpeep + 5` 触发，`PHYS_ALARM_PEEP_LOW` 按 `< refpeep - 3` 触发，每个吸气计划只判断一次。AlarmTask 每 10 ms 检查恢复：动态 PEEP 严格低于高限或严格高于低限持续至少 200 ms 后解除对应报警；等于阈值会中断恢复计时。恢复不使用实时患者压力。无上一完整周期时不触发，停止通气或进入零点补偿阶段时清除 PEEP 报警和计时。检测器仅由 AlarmTask 调用，通过 RTOS 临界区读取 VentTask 的计划、阶段和监测快照，不用于 ISR。

泄漏估计按 `涡轮 → inpFlowSensor → 呼气阀排气支路接点 → midFlowSensor → lung` 的气路定义；呼出气体反向经过 midFlowSensor 后由呼气阀排出。仅 midFlowSensor 下游泄漏计入患者侧补偿，`inpFlow - midFlow` 包含呼气阀正常排气，不能直接作为暂停的近端流量目标。

Monitor 按实际吸气计划的 sequence 切分泄漏累计窗口，必须观察到呼气阶段才结算上一周期，不依赖 midFlow 是否出现负值。原有 VTi/VTe 的负流量呼气确认逻辑保留，与泄漏窗口独立。停止、零点补偿阶段、无有效计划或近端流量重新调零时清除旧估计及未完成周期；吸气中失效后等待下一次吸气边界，第一完整周期结束前使用零补偿。任一样本流量/压力非有限或压力不大于零，整个估计窗口无效；该窗口结束时清除旧 K、回退零补偿，不用删减后的样本发布估计。

`MONITOR_LEAK_BALANCE_COEFFICIENT` 保存有效窗口计算的带符号原始比值；`MONITOR_LEAK_COEFFICIENT` 是限幅到 0..50 的非负 K；`MONITOR_LEAK_VALID` 表示上一窗口通过完整性与数值检查。`MONITOR_LEAK_FLOW` 为 K 乘当前患者压力平方根后的统一补偿量，暂停控制与平台压零流量判定共同使用。独立上限 `MONITOR_PATIENT_LEAK_FLOW_MAX_LPM` 暂保留原值 120 L/min，尚非重新验证的台架限值。无效窗口的原始比值置零，应结合有效标志读取。

上述有效标志不证明肺内储气量首尾相同，也不检测传感器陈旧数据；固定 6 ms、周期首尾储气量近似一致和漏口特性近似恒定仍是估计前提。净储气变化及传感器偏差仍可能被识别为泄漏。暂停期不根据瞬时 midFlow 重新学习 K，避免将供气尾流变成补偿目标。

VAC 容量外环沿用 `MONITOR_TIDA_VOL_INSP` 的近端 VTI 定义（含吸气暂停阶段，未扣患者侧泄漏）。Phase Controller 在加载下一次吸气计划前调用 `monitorEngineBreathComplete()`，结算上一完整周期并将 VTI 送入 Scheduler；Monitor 的 sequence 边界处理保留为补充入口，通过完成标志避免重复发布。`monitorEngineProcess()` 仍仅按顺序调用处理函数。只有同一配置代次、当前计划序号且未消费过的反馈可以参与下一计划，旧配置结果不会重新激活补偿。

首次有效 VTI 直接初始化 `filteredVtiMl`，之后执行 `filteredVtiMl += 0.5 * (vtiMl - filteredVtiMl)`。同时按相同 alpha 平滑产生该 VTI 的计划补偿量 `filteredAppliedCorrectionMl`。外环误差为 `target - filteredVti - (currentCorrection - filteredAppliedCorrection)`，扣除已经施加但尚未反映在 VTI 均值中的补偿，避免重复追补历史欠量。超过用户目标 0.5% 的死区时，首次有效周期以增益 0.8、后续以增益 1.0 更新补偿，每周期步长不超过目标量 25%，累计补偿不超过 ±30%。首次有效反馈同时恢复压力上限快照，避免启动调零发生在初始计划加载后时，下一计划误清除首周期补偿。内部 `deliveryTargetMl = targetTidalVolumeMl + volumeCorrectionMl`，沿用有效供气时间、上升沿面积损失和启动损失公式计算流量；用户目标与暂停时长不变。宏位于 `breathscheduler.h`，当前台架验证范围见 `build/vti_rtt/`。

无有效完整周期、非有限/非正 VTI、未确认实际呼气、非正常时间切换、供气段压力/流量/风机上限或控制器失败时，外环不更新 EMA 和补偿。`BREATH_RESULT_VOLUME_LIMITED` 标记被限幅的周期；非有限采样清除结果中的 `BREATH_RESULT_VALID_VTI`。停机、重新启动、模式或 VAC 设置变化、重新调零会清理外环；压力上限改变时下一计划清理旧补偿。容量修正仅作用于下一周期供气段，暂停目标仍由患者侧泄漏估计决定。

`vt status` 的 `VT_VOLUME_FEEDBACK` 行输出当前计划序号、用户目标、平滑 VTI、补偿量和内部供气目标；体积字段均为 mL 的百分之一。`test_vti_compensation.py` 验证 EMA、实际周期结算时序、重复/旧反馈、限幅、配置重置和固定 80 mL 损失的多周期收敛；软件模型通过不等于已验证实机肺容量或气路稳定性。

VAC 供气流量闭环与 VTI 使用同一个患者侧近端流量 `MDIFF_REAL_FLOW`，不使用入口流量作为反馈。`vt status` 波形追加 `flow_ref_lpm`、`flow_measurement_lpm`、`flow_effort` 和 `flow_blower_ff`（均放大 100 倍），用于直接检查参考跟随、PID 输出和前馈。叠加前馈后的风机指令发生限幅时，控制器撤回同方向积分增量，避免执行器限幅造成的积分饱和。

RTT 启动回归（500 mL、PEEP 5、Ti 2000 ms、15 次/分、无暂停）保存在 `build/vti_rtt/comparison.json` 及对应目录的原始日志/波形。原版本第 7 次结果才到 497 mL（约 30.4 s）；修正后两次独立启动从第 2 次起均在 475..525 mL 内（约 10.3 s）。首次进入 490..510 mL 的结果时间分别约 10.3 s 和 18.4 s，后续仍有约 488..518 mL 波动，不能解读为每次均满足 ±1%。时间从 run 命令到完整周期结果接收，包含初始呼气及呼气结束后发布的等待。测试结束均收到停止确认，未覆盖其他管路、PEEP 或暂停比例。

VAC 供气段压力前馈使用 `Pff = PEEP + Vref*deliveryTarget/userTarget/30 + 0.1572*Qref + 0.004013*Qref*Qref`，压力和流量单位分别为 cmH₂O 与 L/min。30 mL/cmH₂O 是当前模拟肺 250/500/750 mL RTT 数据统一使用的台架顺应性；不能用潮气量反推顺应性，否则高 VT 会被错误解释为更软的肺。`PHASE_REF_VOLUME` 是现有分段流量上升曲线累计面积按整个供气窗口归一化后的用户目标容量轨迹（mL），吸气入口为零、供气结束为用户 VT，支持短于上升沿的供气窗口。控制器按 `deliveryTarget/userTarget` 将外环补偿同步映射到弹性压力前馈，避免只提高流量参考却在吸气末段缺少相应压力。模型保留低增益 PI、绝对压力/转速上限和 VTI 限幅反馈；暂停段继续使用原有实测压力前馈与泄漏流量控制。

VAC 吸气暂停继续使用近端流量反馈，不锁定患者压力。入口采用 Kp=0.003、Kd=0.00005、Ki=0 制动供气尾段；至少经过 120 ms，且近端流量下降到泄漏目标上方 2 L/min 以内后，开启 Ki=0.02 的积分补偿。此时按患者压力一次性选择稳定段参数：低于 30 cmH₂O 保留 Kp=0.003、Kd=0.00005；达到或超过 30 cmH₂O 使用 Kp=0.0003、Kd=0，减少高压力工况的反馈振荡。本次暂停内不反复切换增益。暂停风机指令每 6 ms 最多变化 40 个指令单位，绝对压力对应转速上限优先于变化率限制，限幅时撤回同方向积分增量。患者侧漏气才计入近端流量目标，呼气阀侧流量不能直接当作患者侧漏气目标。验证重点是暂停稳定后 patflow 相对泄漏补偿目标的偏差与振幅，不以原始过零次数作为验收标准。
