/**
**********************************************************************************
* @file     : ProtoclOfPackets.c
* @brief    : 通信协议数据包处理实现
* @details  : 实现通信的协议包收包解码、发包组包等核心功能
* @author   : \.
* @date     : 2025-01-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************
*/
#include "ProtoclOfPackets.h"
#include <string.h>
#include <stddef.h>
//#include "SEGGER_RTT.h"
#include "ProtoclOfRingBuffer.h"
/* ==================== 静态变量定义 ==================== */


/* ==================== 数据包解析函数 ==================== */
ProtocolStatus_t ProtocolParsePacket(const uint8_t* buffer, uint16_t bufferSize, ProtocolPacket_t* packet)
{
    if (!buffer || !packet || bufferSize < PROTOCOL_HEADER_SIZE + PROTOCOL_CRC_SIZE) {
        return PROTOCOL_ERROR;
    }
    
    uint16_t index = 0;
    
    /* 解析帧头 */
    packet->m_header = buffer[index]<< 8 | (buffer[index + 1] );
    index += 2;
    
    /* 解析主ID */
    packet->m_mainId = buffer[index++];
    
    /* 解析ack字节 */
    packet->m_ack = buffer[index++];
    packet->m_needAck = (packet->m_ack != 0);
    
    /* 解析主ID长度 */
    packet->m_mainIdLength = buffer[index++];
    
    /* 解析主ID数据 */
    packet->m_subIdCount = 0;
    packet->m_payloadSize = 0;
    packet->m_mainIdDirectSize = 0;
    
    uint16_t remainingLength = packet->m_mainIdLength;
    uint16_t dataIndex = index;
    
    /* 解析子ID */
    while (remainingLength > 0 && packet->m_subIdCount < PROTOCOL_MAX_SUB_ID_COUNT) {
        if (remainingLength < 2) {
            break;  /* 没有足够的数据用于子ID + 长度 */
        }
        
        uint8_t subId = buffer[dataIndex++];
        uint8_t length = buffer[dataIndex++];
        remainingLength -= 2;
        
        if (remainingLength < ((length >> 3) & 0x1F)) {  /* 检查数据长度 */
            break;
        }
        
        ProtocolSubId_t* subIdPtr = &packet->m_subIds[packet->m_subIdCount];
        subIdPtr->m_subId = subId;
        subIdPtr->m_length = (length >> 3) & 0x1F;
        subIdPtr->m_isValid = (length & 0x01) != 0;
        subIdPtr->m_scale = (length >> 1) & 0x03;
        subIdPtr->m_dataSize = (length >> 3) & 0x1F;
        
        if (subIdPtr->m_dataSize > 0) {
            if ((packet->m_payloadSize + subIdPtr->m_dataSize) > PROTOCOL_MAX_PAYLOAD_SIZE) {
                return PROTOCOL_BUFFER_FULL;
            }
            if (dataIndex + subIdPtr->m_dataSize > bufferSize) {
                return PROTOCOL_ERROR;
            }
            subIdPtr->m_dataOffset = packet->m_payloadSize;
            memcpy(&packet->m_payload[packet->m_payloadSize], &buffer[dataIndex], subIdPtr->m_dataSize);
            packet->m_payloadSize += subIdPtr->m_dataSize;
            dataIndex += subIdPtr->m_dataSize;
            remainingLength -= subIdPtr->m_dataSize;
        }
        
        packet->m_subIdCount++;
    }
    
    /* 解析剩余的主ID数据 */
    if (remainingLength > 0) {
        if ((packet->m_payloadSize + remainingLength) > PROTOCOL_MAX_PAYLOAD_SIZE) {
            return PROTOCOL_BUFFER_FULL;
        }
        if (dataIndex + remainingLength > bufferSize) {
            return PROTOCOL_ERROR;
        }
        packet->m_mainIdDirectOffset = packet->m_payloadSize;
        memcpy(&packet->m_payload[packet->m_payloadSize], &buffer[dataIndex], remainingLength);
        packet->m_payloadSize += remainingLength;
        packet->m_mainIdDirectSize = (uint8_t)remainingLength;
    }
    
    /* 获取crc */
    packet->m_crc = buffer[bufferSize - 2] | (buffer[bufferSize - 1] << 8);
    
    return PROTOCOL_OK;
}

/* ==================== CRC计算函数 ==================== */
uint16_t Crc16Compute(const uint8_t *Data, uint16_t Length) 
{
    uint16_t Crc = 0xFFFF;              //!< initialize
    uint16_t Polynomial = 0x8005;       //!< polynomial：x^16 + x^15 + x^2 + 1

    for (uint16_t i = 0; i < Length; i++) 
    {
        Crc ^= (uint16_t)Data[i] << 8;
        
        for (uint8_t j = 0; j < 8; j++) 
        {
            if (Crc & 0x8000) 
            {
                Crc = (Crc << 1) ^ Polynomial;
            } 
            else 
            {
                Crc <<= 1;
            }
        }
    }
    
    return Crc;
}

bool ProtocolVerifyCrc(const uint8_t* data, uint16_t length, uint16_t crc)
{
    return (Crc16Compute(data, length) == crc);
}

bool ProtocolCheckCRC(const uint8_t* buffer)
{
    if (!buffer) {
        return false;
    }
    
    uint16_t packetSize = ProtocolGetPacketSize(buffer);
    if (packetSize < PROTOCOL_HEADER_SIZE + PROTOCOL_CRC_SIZE) {
        return false;
    }
#if PROTOCOL_ENABLE_CRC_CHECK
    /* 计算CRC */
    uint16_t receivedCrc = buffer[packetSize - 2] | (buffer[packetSize - 1] << 8);
    return ProtocolVerifyCrc(buffer+PROTOCOL_HEADER_SIZE, packetSize - PROTOCOL_CRC_SIZE - PROTOCOL_HEADER_SIZE, receivedCrc);
#else
    return true;
#endif
}
/* ==================== 发送数据创建函数 ==================== */
uint16_t ProtocolCreateSubIdData(uint8_t *Buffer,uint16_t Head,bool needAck,uint8_t mainId, const uint8_t* subIds,uint8_t count)
{
    uint16_t index = 0;
    SubIDCache_t *subIdArray = (SubIDCache_t *)subIds;
    if (!subIdArray || count == 0 || Buffer == NULL) {
        return PROTOCOL_ERROR;
    }
    Buffer[index++] = (Head >> 8) & 0xFF;
    Buffer[index++] = Head & 0xFF;
    Buffer[index++] = mainId;
    Buffer[index++] = needAck ? 0x01 : 0x00;
    uint16_t mainIdLengthIndex = index++;
    uint16_t mainIdLength = 0;
    for (uint8_t i = 0; i < count; i++) {
        Buffer[index++] = subIdArray[i].m_id;
        Buffer[index++] = ProtocolGetSubIdLength(subIdArray[i].m_size, true, subIdArray[i].m_scale);
        for (uint8_t j = 0; j < subIdArray[i].m_size; j++) {
            Buffer[index++] = (subIdArray[i].m_value >> (j * 8)) & 0xFF;
        }
        mainIdLength += 2 + subIdArray[i].m_size;
    }
    Buffer[mainIdLengthIndex] = mainIdLength;
    uint16_t crc = Crc16Compute(&Buffer[2], index - 2);
    Buffer[index++] = crc & 0xFF;
    Buffer[index++] = (crc >> 8) & 0xFF;
    return index; 
}

uint16_t ProtocolCreateDirectData(uint8_t *Buffer,uint16_t Head,bool needAck,uint8_t mainId, const uint8_t* data,uint8_t dataSize)
{
    uint16_t index = 0;
    if (Buffer == NULL) {
        return PROTOCOL_ERROR;
    }
    Buffer[index++] = (Head >> 8) & 0xFF;
    Buffer[index++] = Head & 0xFF;
    Buffer[index++] = mainId;
    Buffer[index++] = needAck ? 0x01 : 0x00;
    Buffer[index++] = dataSize;
    memcpy(&Buffer[index], data, dataSize);
    index += dataSize;
    uint16_t crc = Crc16Compute(&Buffer[2], index - 2);
    Buffer[index++] = crc & 0xFF;
    Buffer[index++] = (crc >> 8) & 0xFF;   
    return index; 
}

/* ==================== 子ID长度计算函数 ==================== */

uint8_t ProtocolGetSubIdLength(uint8_t dataSize, bool isValid, uint8_t scale)
{
    uint8_t length = 0;
    
    /* 位0: 有效位 */
    if (isValid) {
        length |= 0x01;
    }
    
    /* 位1-2: 缩放因子 (2位) */
    length |= (scale & 0x03) << 1;
    
    /* 位3-7: 数据长度 (5位) */
    length |= (dataSize & 0x1F) << 3;
    
    return length;
}

/* ==================== 数据包参数获取函数 ==================== */
uint16_t ProtocolGetPacketHeader(const uint8_t* buffer)
{
    if (!buffer) {
        return 0;
    }
    
    return buffer[0]<< 8 | (buffer[1] );
}

bool ProtocolIsAckRequired(const uint8_t* buffer)
{
    if (!buffer) {
        return false;
    }
    
    /* Ack字节位于第3个字节 */
    uint8_t ackByte = buffer[3];
    return (ackByte != 0);
}

uint16_t ProtocolGetPacketSize(const uint8_t* buffer)
{
    if (!buffer) {
        return 0;
    }
    
    /* 主ID长度位于第4个字节 */
    uint8_t mainIdLength = buffer[4];
    
    /* 总包大小 = 头部(2) + 主ID(1) + Ack(1) + 主ID长度(1) + 主ID数据(mainIdLength) + CRC(2) */
    return PROTOCOL_HEADER_SIZE + 1 + 1 + 1 + mainIdLength + PROTOCOL_CRC_SIZE;
}

uint16_t ProtocolGetCRC(const uint8_t* buffer)
{
    if (!buffer) {
        return 0;
    }
    
    uint16_t packetSize = ProtocolGetPacketSize(buffer);
    if (packetSize < PROTOCOL_HEADER_SIZE + PROTOCOL_CRC_SIZE) {
        return 0;
    }
    
    return buffer[packetSize - 2] | (buffer[packetSize - 1] << 8);
}

uint16_t ProtocolGetScale(uint8_t scale)
{
    switch(scale) {
        case 0: return 1;        // x1
        case 1: return 10;       // x10
        case 2: return 100;      // x100
        case 3: return 1000;     // x1000
        default: return 1;       // 默认x1
    }
}

/**************************End of file********************************/

