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
| `develop/` | VS Code Device Tool 的 CMake 构建、烧录、复位与 RTT 工具；`test_flow_pause.py` 使用真实控制器与模拟输入验证 VAC 暂停控制 |

`vt volume <peep> <ml> [pause_pct]` 通过 RTT 设置 VAC 参数；暂停百分比为 0..99，省略时保留当前值，上电默认 0。`vt volume 15 500 0` 设置无暂停，供气覆盖完整吸气时间，目标流量按有效供气时间计算，`vt run 1` 启动；`vt status` 波形的 `volume_pause` 标记暂停阶段，`pause_settled` 标记已切入稳定段 PI，`leak_lpm` 记录患者侧泄漏估计。`develop/test_vac_matrix.py` 经 Device Tool RTT 入口完成模拟肺九组测试，保存原始日志、波形及逐呼吸振幅、泄漏目标误差统计；过零次数仅作辅助诊断。
| `FreeRTOSConfig.h` | FreeRTOS 工程配置 |

项目代码只能通过 `rtos.h` 使用任务、调度、tick 和临界区能力；FreeRTOS 原生 API 仅允许出现在 `portrtos.c`。日志统一使用 `LOG_I`、`LOG_W`、`LOG_E` 等宏，不能直接使用标准库输出函数。

当前 GD32F470 板载 HXTAL 为 8 MHz，系统使用 `240M_PLL_8M_HXTAL` 配置；该配置决定 RTOS tick 和 APB 外设（包括 VCM UART 230400）的实际时基。

VAC 吸气暂停继续使用近端流量反馈，不锁定患者压力。入口采用 Kp=0.003、Kd=0.00005、Ki=0 制动供气尾段；至少经过 120 ms，且近端流量下降到泄漏目标上方 2 L/min 以内后，开启 Ki=0.02 的积分补偿。此时按患者压力一次性选择稳定段参数：低于 30 cmH₂O 保留 Kp=0.003、Kd=0.00005；达到或超过 30 cmH₂O 使用 Kp=0.0003、Kd=0，减少高压力工况的反馈振荡。本次暂停内不反复切换增益。暂停风机指令每 6 ms 最多变化 40 个指令单位，绝对压力对应转速上限优先于变化率限制，限幅时撤回同方向积分增量。患者侧漏气才计入近端流量目标，呼气阀侧流量不能直接当作患者侧漏气目标。验证重点是暂停稳定后 patflow 相对泄漏补偿目标的偏差与振幅，不以原始过零次数作为验收标准。
