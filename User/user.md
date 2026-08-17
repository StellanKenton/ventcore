# User 层

当前 User 层只保留 RTOS 启动与任务骨架，不包含板级外设驱动。

| 路径 | 职责 |
|---|---|
| `main.c` | 创建 taskManager 并启动调度器 |
| `app/taskmanager.*` | 创建 defaultTask、VentTask、SensorTask、HMITask、AlarmTask |
| `module/rtos/rtos.*` | 项目层 RTOS 最小接口 |
| `module/rtos/portrtos.*` | FreeRTOS 原生接口绑定 |
| `develop/` | VS Code Device Tool 的 CMake 构建、烧录、复位与 RTT 工具 |
| `FreeRTOSConfig.h` | FreeRTOS 工程配置 |

项目代码只能通过 `rtos.h` 使用任务和调度能力；FreeRTOS 原生 API 仅允许出现在 `portrtos.c`。
