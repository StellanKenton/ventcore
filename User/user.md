# User 层

当前 User 层包含板级外设驱动、RTOS 启动、任务骨架和基于 SEGGER RTT 的日志/控制台系统。

| 路径 | 职责 |
|---|---|
| `main.c` | 初始化日志和控制台、注册 worker tasks 并启动调度器 |
| `app/taskmanager.*` | 通过 `WorkerTasksRegister()` 创建 defaultTask、VentTask、SensorTask、HMITask、AlarmTask；全部任务使用绝对周期 `DelayUntil`，SensorTask 每 2 ms 读取两只 SFM3119 |
| `bsp/adc/adc.*` | 使用 ADC1 规则组扫描、连续转换和 DMA1 循环模式持续采集 14 路板级模拟量 |
| `bsp/dvalve/dvalve.*` | 以枚举选择氧气阀、泄压阀或呼气阀，提供统一的 20 kHz、0～100% PWM 占空比控制接口 |
| `bsp/sf06sdk/sfm3119.*` | 管理两只 SFM3119；空气通道使用 PB6/PB7 硬件 I2C0，氧气通道使用 PA12/PA11 模拟 I2C，并缓存原始量、工程量、状态、产品 ID 与序列号 |
| `bsp/valve/valve.*` | 初始化 4 路零点阀控制输出和状态反馈输入，并提供按阀门枚举访问的接口 |
| `module/log/` | RTT 日志、ringbuffer 输出队列和 `help/time/reboot` 控制台命令 |
| `module/rtos/rtos.*` | 项目层任务、调度、固定周期 `DelayUntil`、tick 和临界区接口 |
| `module/rtos/portrtos.*` | FreeRTOS 原生接口绑定 |
| `tools/ringbuffer/` | 日志输出使用的轻量级字节环形缓冲区 |
| `develop/` | VS Code Device Tool 的 CMake 构建、烧录、复位与 RTT 工具 |
| `FreeRTOSConfig.h` | FreeRTOS 工程配置 |

项目代码只能通过 `rtos.h` 使用任务、调度、tick 和临界区能力；FreeRTOS 原生 API 仅允许出现在 `portrtos.c`。日志统一使用 `LOG_I`、`LOG_W`、`LOG_E` 等宏，不能直接使用标准库输出函数。
