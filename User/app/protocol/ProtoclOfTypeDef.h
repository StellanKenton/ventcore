/**
**********************************************************************************
* @file     : ProtoclOfTypeDef.h
* @brief    : 通信协议数据类型和ID定义
* @details  : 定义与通信相关的所有数据类型、ID定义和结构体
* @author   : \.
* @date     : 2025-01-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************
*/

#ifndef _PROTOCOL_TYPEDEF_H_
#define _PROTOCOL_TYPEDEF_H_

#include <stdint.h>
#include <stdbool.h>
#include "ringbuffer.h"
#include "uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 协议状态定义 ==================== */

typedef enum {
    PROTOCOL_OK = 0,
    PROTOCOL_ERROR,
    PROTOCOL_BUSY,
    PROTOCOL_TIMEOUT,
    PROTOCOL_CRC_ERROR,
    PROTOCOL_INVALID_PACKET,
    PROTOCOL_BUFFER_FULL
} ProtocolStatus_t;

/* ==================== 协议数据结构定义 ==================== */

/* 子ID数据结构 */
typedef struct {
    uint8_t m_subId;              /* 子ID号 */
    uint8_t m_length;             /* 长度字段 (有效位 + 缩放位 + 数据长度) */
    uint16_t m_dataOffset;        /* 数据在payload中的偏移 */
    uint8_t m_dataSize;           /* 实际数据大小 */
    bool m_isValid;               /* 数据有效性标志 */
    uint8_t m_scale;              /* 缩放因子 (0:x1, 1:x10, 2:x100, 3:x1000) */
} ProtocolSubId_t;

/* 协议数据包结构 */
typedef struct {
    uint16_t m_header;              /* 帧头地址 */
    uint8_t m_mainId;               /* 主ID */
    uint8_t m_ack;                  /* 应答字段: 0x00不需要应答, 0x01需要应答 */
    uint8_t m_mainIdLength;         /* 主ID数据长度 (不包括ack字节) */
    ProtocolSubId_t m_subIds[32];   /* 子ID数组 */
    uint8_t m_subIdCount;           /* 子ID数量 */
    uint8_t m_payload[128];         /* 统一数据缓冲区 */
    uint16_t m_payloadSize;         /* 已使用的payload大小 */
    uint16_t m_mainIdDirectOffset;  /* 直接主ID数据偏移 (如果有) */
    uint8_t m_mainIdDirectSize;     /* 直接主ID数据大小 (如果有) */
    uint16_t m_crc;                 /* CRC校验 */
    bool m_needAck;                 /* 是否需要应答 */
} ProtocolPacket_t;

/* 协议缓冲区结构 */
typedef struct {
    uint8_t m_buffer[256];          /* 缓冲区 */
    uint16_t m_size;                /* 缓冲区大小 */
    uint16_t m_index;               /* 当前索引 */
} ProtocolBuffer_t;

/* 协议状态结构 */
typedef struct {
    ProtocolBuffer_t m_rxBuffer;    /* 接收缓冲区 */
    ProtocolBuffer_t m_txBuffer;    /* 发送缓冲区 */
    bool m_isRxBusy;                /* 接收忙标志 */
    bool m_isTxBusy;                /* 发送忙标志 */
    uint16_t m_rxWaitCount;         /* 接收等待计数 */
    uint32_t m_rxTimeout;           /* 接收超时 */
    uint32_t m_txTimeout;           /* 发送超时 */
    uint32_t m_ackTimeout;          /* 应答超时时间 */
    uint16_t m_retryCount;          /* 重试计数 */
    uint16_t m_retryInterval;       /* 重试间隔 */
    uint16_t m_maxRetries;          /* 最大重试次数 */  
} ProtocolState_t;

/* 应答结构 */
typedef struct {
    bool m_isWaitingAck;           /* 是否正在等待应答 */
    uint8_t ReSendBuffer[256];     /* 重发数据缓冲区 */
    uint16_t m_bufferSize;         /* 重发数据大小 */
    uint8_t m_retryCount;          /* 重试计数 */
    uint16_t m_ackCRC;             /* 应答包的CRC */
    uint32_t m_ackTimeout;         /* 应答超时时间 */       
} AckPackStatus_t;

typedef struct {
    uint8_t m_id;                /* 子ID */
    uint8_t m_size;                 /* 数据大小 (1或2字节) */
    uint64_t m_value;               /* 参数值 */
    uint8_t m_scale;                /* 缩放因子 (0-3) */
    bool m_valid;                   /* 有效性 */
} SubIDCache_t;

/* 简化的发送队列 */
typedef struct {
    ProtocolPacket_t m_queue[32];   /* 发送队列 */
    uint8_t m_head;                 /* 队列头 */
    uint8_t m_tail;                 /* 队列尾 */
    uint8_t m_count;                /* 队列大小 */
} SendQueue_t;

/* 串口故障联合体 */
//帮助判断串口缓冲区是否设置过小
typedef union {
    uint8_t all;
    struct {
        uint8_t HighpriBufferFull : 1;       /* 高优先级缓冲区满 */
        uint8_t NormalpriBufferFull : 1;     /* 普通优先级缓冲区满 */
        uint8_t LowpriBufferFull : 1;        /* 低优先级缓冲区满 */
        uint8_t RxRingBufferFull : 1;        /* 接收环形缓冲区满 */
        uint8_t AckStateFull : 1;            /* 应答状态满 */
        uint8_t NoAckReceived : 1;           /* 未收到应答 */
        uint8_t Reserved : 2;                /* 保留位 */
    } bits;
} UsartError_t; 

/*串口处理类*/
typedef struct {
    /* 发送缓冲区 */
    stRingBuffer m_highPriority;                                   /* 高优先级队列   */
    uint8_t *m_highPriorityBuffer;                          /* 高优先级队列缓冲区 */
    uint16_t m_highPriority_Size;                           /* 高优先级队列大小 */
    stRingBuffer m_normalPriority;                                 /* 普通优先级队列 */
    uint8_t *m_normalPriorityBuffer;                        /* 普通优先级队列缓冲区 */
    uint16_t m_normalPriority_Size;                         /* 普通优先级队列大小 */

    /* 接收缓冲区 */
    stRingBuffer m_RxRingBuffer;                                   /* 接收环形缓冲区 */
    uint8_t *m_RxRBStorage;                                 /* 接收环形缓冲区存储 */
    uint16_t m_RxRBSize;                                    /* 接收环形缓冲区大小 */

    /* 通信状态 */
    ProtocolState_t m_protocolState;                        /* 通信状态 */

    /* 回调函数 */
    void (*m_txCallbacks)(uint8_t instance);        /* 发送回调 */
    void (*m_rxCallbacks)(const ProtocolPacket_t* packet);  /* 接收回调 */

    /* 统计信息 */
    uint32_t m_txCount;                                     /* 发送计数 */
    uint32_t m_rxCount;                                     /* 接收计数 */
    uint32_t m_errorCount;                                  /* 错误计数 */

    /* 数据帧头 */
    uint16_t m_sHead;                                   
    uint16_t m_rHead;

    /* 应答状态 */
    AckPackStatus_t *m_ackPack;
    uint8_t m_ackNumber;

    /* 串口错误状态 */
    // 如果错误状态为1，则说明对应的缓冲区设置过小
    UsartError_t m_usartError;                                 /* 串口错误状态 */
    
    uint8_t DMASend[256];                                       /* 发送缓冲区 */
}UsartManager_t;

#ifdef __cplusplus
}
#endif

#endif /* _PROTOCOL_MCM_TYPEDEF_H_ */
