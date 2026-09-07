/**
**********************************************************************************
* @file     : ProtoclOfMcmConfig.h
* @brief    : MCM通信协议配置参数和常量定义
* @details  : 定义协议相关的配置参数、常量、缓存尺寸等
* @author   : \.
* @date     : 2025-01-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************
*/

#ifndef _PROTOCOL_CONFIG_H_
#define _PROTOCOL_CONFIG_H_

#include "ProtoclOfTypeDef.h"
//#include "SEGGER_RTT.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 串口配置 ==================== */
#define PROTOCOL_MAX_USART_INSTANCES          1       /* 最大USART实例数量 */

/* ==================== 协议基本配置 ==================== */
/* 环形缓冲区大小 */
// MCM <-> VCM 通信缓冲区定义
#define PROTOCOL_MCM_TO_VCM_RX_BUFFER_SIZE            1024    /* 环形缓冲区大小 */
#define PROTOCOL_MCM_TO_VCM_TX_HIGH_BUFFER_SIZE       512     /* 环形缓冲区大小 */
#define PROTOCOL_MCM_TO_VCM_TX_NORMAL_BUFFER_SIZE     1024    /* 环形缓冲区大小 */
#define PROTOCOL_MCM_TO_VCM_ACK_BUFFER_NUM            5       /* 应答包数量 */
// PCM <-> VCM 通信缓冲区定义
#define PROTOCOL_PCM_TO_VCM_RX_BUFFER_SIZE            256    /* 环形缓冲区大小 */
#define PROTOCOL_PCM_TO_VCM_TX_HIGH_BUFFER_SIZE       256     /* 环形缓冲区大小 */
#define PROTOCOL_PCM_TO_VCM_TX_NORMAL_BUFFER_SIZE     256    /* 环形缓冲区大小 */
#define PROTOCOL_PCM_TO_VCM_TX_LOW_BUFFER_SIZE        256    /* 环形缓冲区大小 */
#define PROTOCOL_PCM_TO_VCM_ACK_BUFFER_NUM            2       /* 应答包数量 */


/* 数据包配置 */
#define PROTOCOL_MAX_PACKET_SIZE         256     /* 最大数据包大小 */
#define PROTOCOL_MAX_SUB_ID_COUNT        32      /* 最大子ID数量 */
#define PROTOCOL_MAX_PAYLOAD_SIZE        128     /* 最大payload大小 */
#define PROTOCOL_HEADER_SIZE             2       /* 帧头大小 */
#define PROTOCOL_HEADER_PACK_SIZE        5       /* 含字节长度的帧头大小 */
#define PROTOCOL_CRC_SIZE                2       /* CRC大小 */

/* CRC配置 */
#define PROTOCOL_CRC_POLYNOMIAL          0x8005  /* CRC多项式 */

/* 超时配置 */
#define PROTOCOL_RX_TIMEOUT_MS           500     /* 接收超时 (ms) */
#define PROTOCOL_TX_TIMEOUT_MS           1000    /* 发送超时 (ms) */
#define PROTOCOL_ACK_TIMEOUT_MS          1000    /* 应答超时 (ms) */
#define PROTOCOL_MAX_RETRIES             3       /* 最大重试次数 */
#define PROTOCOL_RETRY_INTERVAL_MS       100     /* 重试间隔 (ms) */
/* ==================== 功能配置 ==================== */
/* 系统配置 */
#define PROTOCOL_ENABLE_CRC_CHECK        1       /* 启用CRC检查 */
#define PROTOCOL_ENABLE_ACK_MECHANISM    1       /* 启用应答机制 */
#define PROTOCOL_ENABLE_DEBUG            1       /* 启用调试 */
/* ==================== 发送频率配置 ==================== */


/* 发送优先级 */
#define PROTOCOL_PRIORITY_HIGH           0       /* 高优先级 */
#define PROTOCOL_PRIORITY_NORMAL         1       /* 普通优先级 */
#define PROTOCOL_PRIORITY_LOW            2       /* 低优先级 */

/* ==================== 缩放因子配置 ==================== */

/* 缩放因子定义 */
#define PROTOCOL_SCALE_X1                0       /* x1 */
#define PROTOCOL_SCALE_X10               1       /* x10 */
#define PROTOCOL_SCALE_X100              2       /* x100 */
#define PROTOCOL_SCALE_X1000             3       /* x1000 */

/* ==================== 报警配置 ==================== */

/* 报警数据配置 */
#define PROTOCOL_ALARM_DATA_SIZE         4       /* 报警数据大小 */
#define PROTOCOL_MAX_ALARM_TYPES         32      /* 最大报警类型数 */

/* 报警级别定义 */
#define PROTOCOL_ALARM_LEVEL_HIGH        0       /* 高级报警 */
#define PROTOCOL_ALARM_LEVEL_MEDIUM      1       /* 中级报警 */
#define PROTOCOL_ALARM_LEVEL_LOW         2       /* 低级报警 */

/* ==================== 系统配置 ==================== */
/* 任务延时 */
#define PROTOCOL_TASK_DELAY_MS           10      /* 任务延时 (ms) */

/* 联调模式配置 */
#define PROTOCOL_DEBUG_MODE_DISABLE_ACK  1       /* 联调模式：禁用所有应答要求 */
                                                    /* 1: 禁用应答(联调模式) */
                                                    /* 0: 启用应答(正常模式) */

/* ==================== 调试配置 ==================== */

#if PROTOCOL_ENABLE_DEBUG
#define PROTOCOL_DEBUG_PRINT(fmt, ...)  SEGGER_RTT_printf(0, "[PROTOCOL] " fmt "\n", ##__VA_ARGS__)
#define PROTOCOL_RTT_PRINT(fmt, ...)    SEGGER_RTT_printf(0, fmt, ##__VA_ARGS__)
#else
#define PROTOCOL_DEBUG_PRINT(fmt, ...)
#define PROTOCOL_RTT_PRINT(fmt, ...)
#endif

/* ==================== 错误码定义 ==================== */

/* 错误码定义 */
#define PROTOCOL_ERR_NONE                0x00    /* 无错误 */
#define PROTOCOL_ERR_INVALID_PARAM       0x01    /* 无效参数 */
#define PROTOCOL_ERR_BUFFER_FULL         0x02    /* 缓冲区满 */
#define PROTOCOL_ERR_CRC_ERROR           0x03    /* CRC错误 */
#define PROTOCOL_ERR_TIMEOUT             0x04    /* 超时 */
#define PROTOCOL_ERR_BUSY                0x05    /* 忙状态 */
#define PROTOCOL_ERR_INVALID_PACKET      0x06    /* 无效数据包 */
#define PROTOCOL_ERR_NOT_INITIALIZED     0x07    /* 未初始化 */

/* ==================== 功能开关配置 ==================== */

/* 功能开关 */
#define PROTOCOL_FEATURE_WAVE_DATA       1       /* 波形数据功能 */
#define PROTOCOL_FEATURE_MONITOR_PARAMS  1       /* 监测参数功能 */
#define PROTOCOL_FEATURE_ALARM           1       /* 报警功能 */
#define PROTOCOL_FEATURE_CALIBRATION     1       /* 校准功能 */
#define PROTOCOL_FEATURE_DIAGNOSIS       1       /* 诊断功能 */

/* ==================== 性能配置 ==================== */

/* 性能配置 */
#define PROTOCOL_MAX_PROCESSING_TIME_MS  10      /* 最大处理时间 (ms) */
#define PROTOCOL_MAX_QUEUE_DEPTH         32      /* 最大队列深度 */
#define PROTOCOL_MEMORY_POOL_SIZE        1024    /* 内存池大小 */

/* ==================== 兼容性配置 ==================== */

/* 兼容性配置 */
#define PROTOCOL_VERSION_MAJOR           1       /* 主版本号 */
#define PROTOCOL_VERSION_MINOR           0       /* 次版本号 */
#define PROTOCOL_VERSION_PATCH           0       /* 补丁版本号 */

/* 版本字符串 */
#define PROTOCOL_VERSION_STRING          "1.0.0"

/* ==================== 配置验证宏 ==================== */

/* 配置验证 */
#define PROTOCOL_VALIDATE_PACKET_SIZE(size) \
    ((size) <= PROTOCOL_MAX_PACKET_SIZE)

#define PROTOCOL_VALIDATE_SUB_ID_COUNT(count) \
    ((count) <= PROTOCOL_MAX_SUB_ID_COUNT)

#define PROTOCOL_VALIDATE_PAYLOAD_SIZE(size) \
    ((size) <= PROTOCOL_MAX_PAYLOAD_SIZE)

#define PROTOCOL_VALIDATE_SCALE(scale) \
    ((scale) <= PROTOCOL_SCALE_X1000)

/* ==================== 配置初始化函数 ==================== */

/**
 * @brief 初始化协议配置
 * @return ProtocolStatus_t 初始化状态
 */
ProtocolStatus_t ProtocolConfigInit(void);

/**
 * @brief 获取协议版本信息
 * @param major 主版本号指针
 * @param minor 次版本号指针
 * @param patch 补丁版本号指针
 */
void ProtocolGetVersion(uint8_t* major, uint8_t* minor, uint8_t* patch);

/**
 * @brief 验证配置参数
 * @return bool 配置是否有效
 */
bool ProtocolValidateConfig(void);

#ifdef __cplusplus
}
#endif

#endif /* _PROTOCOL_MCM_CONFIG_H_ */
