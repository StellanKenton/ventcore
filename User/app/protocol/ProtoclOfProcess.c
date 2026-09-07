/**
**********************************************************************************
* @file     : ProtoclOfProcess.c
* @brief    : 通信协议调度和缓存处理实现
* @details  : 实现通信的发送数据和接收数据调度，以及接收数据缓存处理，发送数据预处理
* @author   : \.
* @date     : 2025-01-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************
*/

#include <string.h>
//#include "SEGGER_RTT.h"
#include "ProtoclOfProcess.h"
#include "ProtoclOfMcm.h"
//#include "ProtoclOfPcm.h"
// #include "ProtoclOfCalib.h" /* Pending calibration transport port. */
/** Write a complete frame or leave the queue unchanged. Task context only. */
static bool protocolRingWrite(stRingBuffer *buffer, const uint8_t *data, uint16_t length) {
    return ringBufferGetFree(buffer) >= length && ringBufferWrite(buffer, data, length) == length;
}
/* ==================== 静态变量定义 ==================== */

/* 环形缓冲区定义 */
// MCM <-> VCM 通信缓冲区定义
static uint8_t g_VCMtoMCM_HighPriorityBuffer[PROTOCOL_MCM_TO_VCM_TX_HIGH_BUFFER_SIZE];  // 高优先级发送缓冲区
static uint8_t g_VCMtoMCM_NormalPriorityBuffer[PROTOCOL_MCM_TO_VCM_TX_NORMAL_BUFFER_SIZE];  // 普通优先级发送缓冲区
static uint8_t g_MCMtoVCM_RxBuffer[PROTOCOL_MCM_TO_VCM_RX_BUFFER_SIZE];                 // 接收缓冲区
static AckPackStatus_t g_MCMtoVCm_ackPack[PROTOCOL_MCM_TO_VCM_ACK_BUFFER_NUM];          // 应答状态缓冲区
// PCM <-> VCM 通信缓冲区定义
WaveData_t Recv_WaveData;
/* 串口管理初始化 */
// 只需修改下面的初始化参数即可适配不同的USART实例
static UsartManager_t g_UsartMgrs[PROTOCOL_MAX_USART_INSTANCES] = {
    {      
        /* 发送缓冲区 */
        .m_highPriorityBuffer = g_VCMtoMCM_HighPriorityBuffer,
        .m_highPriority_Size = sizeof(g_VCMtoMCM_HighPriorityBuffer),
        .m_normalPriorityBuffer = g_VCMtoMCM_NormalPriorityBuffer,
        .m_normalPriority_Size = sizeof(g_VCMtoMCM_NormalPriorityBuffer),

        /* 接收缓冲区 */
        .m_RxRBStorage = g_MCMtoVCM_RxBuffer,
        .m_RxRBSize = sizeof(g_MCMtoVCM_RxBuffer),

        /* 通信状态 */
        .m_protocolState.m_maxRetries = PROTOCOL_MAX_RETRIES,
        .m_protocolState.m_rxTimeout = PROTOCOL_RX_TIMEOUT_MS,
        .m_protocolState.m_txTimeout = PROTOCOL_TX_TIMEOUT_MS,
        .m_protocolState.m_ackTimeout = PROTOCOL_ACK_TIMEOUT_MS,
        .m_protocolState.m_retryInterval = PROTOCOL_RETRY_INTERVAL_MS,

        /* 回调函数 */
        .m_rxCallbacks = &ProtocolProcessRxPacket,
        .m_txCallbacks = &ProtocolDataPreProcess,

        /* 帧头 */
        .m_sHead = PROTOCOL_ADDR_VCM_TO_MCM,
        .m_rHead = PROTOCOL_ADDR_MCM_TO_VCM,

        /* 应答状态 */
        .m_ackPack = g_MCMtoVCm_ackPack,
        .m_ackNumber = PROTOCOL_MCM_TO_VCM_ACK_BUFFER_NUM,
    },
};

/* ==================== 初始化函数 ==================== */

ProtocolStatus_t ProtocolProcessInit(uint8_t instance)
{
    if (instance >= PROTOCOL_MAX_USART_INSTANCES) { return PROTOCOL_INVALID_PACKET; }
    /* 初始化环形缓冲区 */
    ringBufferInit(&g_UsartMgrs[instance].m_RxRingBuffer,
               g_UsartMgrs[instance].m_RxRBStorage, 
               g_UsartMgrs[instance].m_RxRBSize);
    /* 初始化发送优先级队列 */
    ringBufferInit(&g_UsartMgrs[instance].m_highPriority,
               g_UsartMgrs[instance].m_highPriorityBuffer, 
               g_UsartMgrs[instance].m_highPriority_Size);

    ringBufferInit(&g_UsartMgrs[instance].m_normalPriority,
               g_UsartMgrs[instance].m_normalPriorityBuffer,
               g_UsartMgrs[instance].m_normalPriority_Size);

    /* 初始化统计信息 */
    g_UsartMgrs[instance].m_txCount = 0;
    g_UsartMgrs[instance].m_rxCount = 0;
    g_UsartMgrs[instance].m_errorCount = 0;

    return PROTOCOL_OK;
}

void ProtocolProcessDeInit(void)
{
    memset(&g_UsartMgrs, 0, sizeof(g_UsartMgrs));
}

/* ==================== 主处理函数 ==================== */

void ProtocolProcessMain(uint8_t instance)
{
    if (instance >= PROTOCOL_MAX_USART_INSTANCES) { return; }
    /* 接收数据 */
    ProtocolReceiveData(instance);

    /* 处理数据 */
    for (uint8_t lFrame = 0; lFrame < 16U; ++lFrame) {
        if (ProtocolProcessRxData(instance) != PROTOCOL_OK) { break; }
    }
    
    /* 处理应答超时检测和重发 */
    ProtocolProcessAckTimeout(instance);
      
    /* 发送数据预处理 */
    g_UsartMgrs[instance].m_txCallbacks(instance);

    /* 处理调度器 */
    ProtocolSchedulerProcess(instance);
    
}
/* ==================== 调度器发送函数 ==================== */
void ProtocolSchedulerProcess(uint8_t instance)
{
    if (instance >= PROTOCOL_MAX_USART_INSTANCES) { return; }

    uint8_t *TxBuff;
    TxBuff = g_UsartMgrs[instance].DMASend;
    stRingBuffer *RingBuffer = NULL;
    /* 检查USART是否空闲 - 如果BSP空闲则强制清除忙状态 */
    if (g_UsartMgrs[instance].m_protocolState.m_isTxBusy) {
        // 如果BSP实际空闲，强制清除协议忙状态
        if (!uartIsTxBusy(instance)) {
            g_UsartMgrs[instance].m_protocolState.m_isTxBusy = false;
        } else {
            return; // USART确实忙，等待下次处理
        }
    }
    /* 强制清除忙状态：如果BSP空闲，无论协议状态如何都清除 */
    if (!uartIsTxBusy(instance)) {
        g_UsartMgrs[instance].m_protocolState.m_isTxBusy = false;
    }
    
    /* 优先处理高优先级队列*/
    if (!ringBufferIsEmpty(&g_UsartMgrs[instance].m_highPriority)) {
        RingBuffer = &g_UsartMgrs[instance].m_highPriority;
        
    }
    /* 然后处理普通优先级队列*/
    else if (!ringBufferIsEmpty(&g_UsartMgrs[instance].m_normalPriority)) {
        RingBuffer = &g_UsartMgrs[instance].m_normalPriority;
    }

    // 处理发送数据
    if(RingBuffer != NULL) {
        ringBufferPeek(RingBuffer, (uint8_t *)TxBuff, PROTOCOL_HEADER_PACK_SIZE);
        uint16_t head = ProtocolGetPacketHeader((uint8_t *)TxBuff);
        uint16_t packetSize = ProtocolGetPacketSize((uint8_t *)TxBuff);
#if PROTOCOL_ENABLE_ACK_MECHANISM
        uint8_t ackNeeded = 0; 
        /* A heartbeat reply is terminal; do not request an ACK of an ACK.
         * Retain its existing wire ACK byte without scheduling unsolicited retries. */
        if(head == g_UsartMgrs[instance].m_sHead && TxBuff[2] != PROTOCOL_TX_MID_HEARTBEAT)
            ackNeeded = ProtocolIsAckRequired((uint8_t *)TxBuff);
#endif // PROTOCOL_ENABLE_ACK_MECHANISM
        ringBufferPeek(RingBuffer, (uint8_t *)TxBuff, packetSize);
        if (uartSendData(instance, (uint8_t *)TxBuff, packetSize) == UART_STATUS_OK) {
            g_UsartMgrs[instance].m_protocolState.m_isTxBusy = true;
#if PROTOCOL_ENABLE_ACK_MECHANISM
            if (ackNeeded) {
                ProtocolRegisterAck(instance, (uint8_t *)TxBuff, packetSize);
            }
#endif // PROTOCOL_ENABLE_ACK_MECHANISM
            ringBufferRead(RingBuffer, (uint8_t *)TxBuff, packetSize);
            g_UsartMgrs[instance].m_txCount++;
            if (head == PROTOCOL_ADDR_VCM_TO_MCM && TxBuff[2] == PROTOCOL_TX_MID_HEARTBEAT) {
                protocolHeartbeatTransmitted();
            }
        }
    }
    
}

ProtocolStatus_t ProtocolSendData(uint8_t instance,uint8_t Priority, uint8_t* pData, uint16_t length)
{
    ProtocolStatus_t status = PROTOCOL_ERROR; 
    if (instance >= PROTOCOL_MAX_USART_INSTANCES || !pData || length < 7U || length > PROTOCOL_MAX_PACKET_SIZE || ProtocolGetPacketSize(pData) != length) {
        return PROTOCOL_INVALID_PACKET;
    }
    switch (Priority) {
        case PROTOCOL_PRIORITY_HIGH:
            if(protocolRingWrite(&g_UsartMgrs[instance].m_highPriority, pData, length)) {
                status = PROTOCOL_OK;
            } else {
                status = PROTOCOL_BUFFER_FULL;
                g_UsartMgrs[instance].m_usartError.bits.HighpriBufferFull = 1;
            }
            break;
        case PROTOCOL_PRIORITY_NORMAL:
            if(protocolRingWrite(&g_UsartMgrs[instance].m_normalPriority, pData, length)) {
                status = PROTOCOL_OK;
            } else {
                status = PROTOCOL_BUFFER_FULL;
                g_UsartMgrs[instance].m_usartError.bits.NormalpriBufferFull = 1;
            }
            break;
        case PROTOCOL_PRIORITY_LOW:
            // if(protocolRingWrite(&g_UsartMgrs[instance].m_lowPriority, pData, length)) {
            //     status = PROTOCOL_OK;
            // } else {
            //     status = PROTOCOL_BUFFER_FULL;
            //     g_UsartMgrs[instance].m_usartError.bits.LowpriBufferFull = 1;
            // }
            break;
        default:
            // 无效优先级，忽略
            break;
    }
    return status;
}
/* ==================== 数据包接收函数 ==================== */
ProtocolStatus_t ProtocolReceiveData(uint8_t instance)
{
    if (instance >= PROTOCOL_MAX_USART_INSTANCES) { return PROTOCOL_INVALID_PACKET; }
    // Receive data from USART into ring buffer
    uint8_t rxBuffer[256];
    uint16_t rxSize = uartGetRxDataCount(instance);
    
    if (rxSize > sizeof(rxBuffer)) { rxSize = sizeof(rxBuffer); }
    if (rxSize > ringBufferGetFree(&g_UsartMgrs[instance].m_RxRingBuffer)) {
        rxSize = ringBufferGetFree(&g_UsartMgrs[instance].m_RxRingBuffer);
    }
    if (rxSize == 0) {
        return PROTOCOL_OK;
    }
    
    int8_t status = uartGetRxData(instance, rxBuffer, rxSize);
    if (status != UART_STATUS_OK) {
        return PROTOCOL_ERROR;
    }

    if(protocolRingWrite(&g_UsartMgrs[instance].m_RxRingBuffer, rxBuffer, rxSize) != true) {
        return PROTOCOL_ERROR;
    }
    return PROTOCOL_OK;
}

ProtocolStatus_t ProtocolProcessRxData(uint8_t instance)
{
    if (instance >= PROTOCOL_MAX_USART_INSTANCES) { return PROTOCOL_INVALID_PACKET; }
    uint16_t header = 0;
    uint16_t RxDataLen = ringBufferGetUsed(&g_UsartMgrs[instance].m_RxRingBuffer);

    /* 基本长度检查 */
    if(RxDataLen <= PROTOCOL_HEADER_PACK_SIZE) {
        return PROTOCOL_INVALID_PACKET; // 数据不完整，等待更多数据

    }

    /* 循环读取数据直到找到帧头 */
    bool foundHeader = false;
    uint16_t bytesDiscarded = 0;
    
    for (uint16_t i = 0; i <= RxDataLen - 2; i++) {
        ringBufferPeek(&g_UsartMgrs[instance].m_RxRingBuffer, g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer, 2);
        header = (g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[0] << 8) 
                | g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[1];
        if((header == g_UsartMgrs[instance].m_sHead) 
            || (header == g_UsartMgrs[instance].m_rHead)
            /* Calibration transport is not ported. */ ){
            foundHeader = true;
            break;
        } else {
            // 丢弃一个字节继续查找
            uint8_t discardByte;
            ringBufferRead(&g_UsartMgrs[instance].m_RxRingBuffer, &discardByte, 1);
            bytesDiscarded++;
            RxDataLen--; // 更新剩余数据长度
            
            // 如果剩余数据不足，退出循环
            if(RxDataLen <= PROTOCOL_HEADER_PACK_SIZE) {
                return PROTOCOL_INVALID_PACKET;
            }
        }
    }
    
    if(!foundHeader) {
        return PROTOCOL_INVALID_PACKET; // 未找到有效帧头
    }

    /* 读取包头信息 */
    ringBufferPeek(&g_UsartMgrs[instance].m_RxRingBuffer,
                        g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer, PROTOCOL_HEADER_PACK_SIZE);
    uint16_t expectedPacketSize = ProtocolGetPacketSize(g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer);
    if(expectedPacketSize > PROTOCOL_MAX_PACKET_SIZE) {
        // 包长度异常，丢弃包头部数据
        uint8_t discardByte;
        ringBufferRead(&g_UsartMgrs[instance].m_RxRingBuffer, (uint8_t *)&discardByte, 1);
        return PROTOCOL_INVALID_PACKET;
    }
    /* 检查数据包长度 */
    if(RxDataLen < expectedPacketSize) {
        // 等待更多数据
        g_UsartMgrs[instance].m_protocolState.m_rxWaitCount += PROTOCOL_TASK_DELAY_MS;  // 任务周期为10ms
        if(g_UsartMgrs[instance].m_protocolState.m_rxWaitCount >= g_UsartMgrs[instance].m_protocolState.m_rxTimeout) {
            // 超时丢弃数据
            uint8_t discardByte;
            ringBufferRead(&g_UsartMgrs[instance].m_RxRingBuffer, (uint8_t *)&discardByte, 1);
            g_UsartMgrs[instance].m_protocolState.m_rxWaitCount = 0;
            return PROTOCOL_TIMEOUT;
        }
        return PROTOCOL_INVALID_PACKET; // 等待更多数据
    } else if(expectedPacketSize > PROTOCOL_MAX_PACKET_SIZE) {
        uint8_t discardByte;
        ringBufferRead(&g_UsartMgrs[instance].m_RxRingBuffer, (uint8_t *)&discardByte, 1);
        return PROTOCOL_INVALID_PACKET;
    }
    g_UsartMgrs[instance].m_protocolState.m_rxWaitCount = 0; // 重置等待计数器

    /* 读取完整数据包到缓冲区 */
    ringBufferPeek(&g_UsartMgrs[instance].m_RxRingBuffer,
        g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer, expectedPacketSize);

    /* CRC校验 */
    if(!ProtocolCheckCRC(g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer)) {
        // CRC校验失败，丢弃包头部数据
        uint8_t discardByte;
        ringBufferRead(&g_UsartMgrs[instance].m_RxRingBuffer, (uint8_t *)&discardByte, 1);
        return PROTOCOL_CRC_ERROR;
    }

    /* 从环形缓冲区弹出已处理的数据 */
    ringBufferRead(&g_UsartMgrs[instance].m_RxRingBuffer, g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer, expectedPacketSize);
    g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_index = expectedPacketSize;

    if(header == g_UsartMgrs[instance].m_sHead || header == g_UsartMgrs[instance].m_rHead){
        /* 解析数据包 */
        ProtocolPacket_t packet;
        /* Echo ACK frames carry outbound direct data, not MCM sub-IDs. */
        if (header == g_UsartMgrs[instance].m_sHead) {
            memset(&packet, 0, sizeof(packet));
            packet.m_crc = ProtocolGetCRC(g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer);
            ProtocolProcessReceivedAck(instance, &packet);
            return PROTOCOL_OK;
        }
        ProtocolStatus_t parseStatus = ProtocolParsePacket(g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer, expectedPacketSize, &packet);
#if 0 /* Legacy reverse waveform decoder conflicts with MCM parameter ID 0xAF. */
        if(packet.m_mainId == 0xAF) {
            Recv_WaveData.m_breathPhase = g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[5];
            Recv_WaveData.m_pressure = g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[6] | (g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[7] << 8);
            Recv_WaveData.m_flow = g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[8] | (g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[9] << 8);
            Recv_WaveData.m_volume = g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[10] | (g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[11] << 8);
            Recv_WaveData.m_timestamp = g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[12] << 24 | (g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[13] << 16) |
                                        (g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[14] << 8) | g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[15];
            return PROTOCOL_OK;
        }
#endif
        if (parseStatus == PROTOCOL_OK) {
            g_UsartMgrs[instance].m_rxCount++;
            /* 首先检查是否是应答包 */
            if (packet.m_header == g_UsartMgrs[instance].m_sHead) {
    #if PROTOCOL_ENABLE_ACK_MECHANISM
                ProtocolProcessReceivedAck(instance, &packet);
    #endif // PROTOCOL_ENABLE_ACK_MECHANISM
            } else { 
                /* 有效数据包接收 - 调用回调函数 */
                if (g_UsartMgrs[instance].m_rxCallbacks) {
                    g_UsartMgrs[instance].m_rxCallbacks(&packet);
                }
    #if PROTOCOL_ENABLE_ACK_MECHANISM
                if(packet.m_needAck) {
                    ProtocolSendData(instance, PROTOCOL_PRIORITY_HIGH,g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer, expectedPacketSize);
                }
    #endif // PROTOCOL_ENABLE_ACK_MECHANISM
            }

            return PROTOCOL_OK;
        } else {
            return PROTOCOL_ERROR; // 解析失败
        }
    } else {
        /* uint8_t ClaibDataLen = g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer[4];
        ProtoclOfCalibRBuffPush(g_UsartMgrs[instance].m_protocolState.m_rxBuffer.m_buffer+5, ClaibDataLen); */
        return PROTOCOL_OK;
    }
}
/* ==================== 应答处理函数 ==================== */
/**
 * @brief 处理接收到的应答
 * @param packet 接收到的数据包
 */
void ProtocolProcessReceivedAck(uint8_t instance, const ProtocolPacket_t* packet)
{
    if (!packet) {
        return;
    }

    for (uint8_t i = 0; i < g_UsartMgrs[instance].m_ackNumber; i++) {
        AckPackStatus_t* ackState = &g_UsartMgrs[instance].m_ackPack[i];
        if (ackState->m_isWaitingAck == false) {
            continue; // 不在等待应答，检查下一个
        } else {
            if (ackState->m_ackCRC == packet->m_crc) {
                /* 收到应答，清除等待状态 */
                ackState->m_ackTimeout = 0;
                ackState->m_retryCount = 0;
                ackState->m_isWaitingAck = false;

                memset(&ackState->m_ackCRC, 0, sizeof(ackState->m_ackCRC));
                memset(&ackState->ReSendBuffer, 0, sizeof(ackState->ReSendBuffer));                
                ackState->m_bufferSize = 0;
                return;
            }
        }
        
    }
}

void ProtocolProcessAckTimeout(uint8_t instance)
{
#if PROTOCOL_ENABLE_ACK_MECHANISM
   for (uint8_t i = 0; i < g_UsartMgrs[instance].m_ackNumber; i++) {
        AckPackStatus_t* ackPacket = &g_UsartMgrs[instance].m_ackPack[i];
        if (ackPacket->m_isWaitingAck) {
            ackPacket->m_ackTimeout += PROTOCOL_TASK_DELAY_MS;
            if (ackPacket->m_ackTimeout >= g_UsartMgrs[instance].m_protocolState.m_retryInterval) {
                /* 应答超时处理 */
                if (ackPacket->m_retryCount < g_UsartMgrs[instance].m_protocolState.m_maxRetries) {
                    /* 重发数据包 */
                    ProtocolStatus_t sendStatus = ProtocolSendData(instance,PROTOCOL_PRIORITY_HIGH, 
                                                                ackPacket->ReSendBuffer, 
                                                                ackPacket->m_bufferSize);
                    if (sendStatus == PROTOCOL_OK) {
                        ackPacket->m_retryCount++;
                        ackPacket->m_ackTimeout = 0; // 重置超时计数器
                    } else {
                        /* 发送失败，记录错误 */
                        g_UsartMgrs[instance].m_usartError.bits.NoAckReceived = 1;
                    }
                } else {
                    /* 达到最大重发次数，放弃等待应答 */
                    ackPacket->m_isWaitingAck = false;
                    ackPacket->m_ackTimeout = 0;
                    ackPacket->m_retryCount = 0;
                    
                    memset(&ackPacket->m_ackCRC, 0, sizeof(ackPacket->m_ackCRC));
                    memset(&ackPacket->ReSendBuffer, 0, sizeof(ackPacket->ReSendBuffer));                    
                    ackPacket->m_bufferSize = 0;
                }
            }
        }
    }
#endif // PROTOCOL_ENABLE_ACK_MECHANISM
}


void ProtocolRegisterAck(uint8_t instance, const uint8_t* data, uint16_t size)
{
    if (!data || size == 0) {
        return;
    }
    for (uint8_t i = 0; i < g_UsartMgrs[instance].m_ackNumber; i++) {
        AckPackStatus_t* ackState = &g_UsartMgrs[instance].m_ackPack[i];
        if (ackState->m_isWaitingAck == false) {
            /* 找到空闲的应答槽 */
            ackState->m_isWaitingAck = true;
            ackState->m_ackTimeout = 0;
            ackState->m_retryCount = 0;
            ackState->m_ackCRC = ProtocolGetCRC(data);
            memcpy(ackState->ReSendBuffer, data, size);
            ackState->m_bufferSize = size;
            return;
        }else {
            if (ackState->m_ackCRC == ProtocolGetCRC(data)) {
                /* 已经存在相同的应答等待，避免重复注册 */
                return;
            }
        }
    }
    g_UsartMgrs[instance].m_usartError.bits.AckStateFull = 1; // 应答状态缓冲区满
}
/* ==================== End of File ==================== */
