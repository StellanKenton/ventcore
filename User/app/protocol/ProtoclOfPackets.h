/**
**********************************************************************************
* @file     : ProtoclOfPackets.h
* @brief    : MCM通信协议数据包处理头文件
* @details  : 实现与MCM通信的协议包收包解码、发包组包等核心功能
* @author   : \.
* @date     : 2025-01-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************
*/

#ifndef _PROTOCOL_PACKETS_H_
#define _PROTOCOL_PACKETS_H_


#include <stdint.h>
#include <stdbool.h>
#include "ProtoclOfTypeDef.h"
#include "ProtoclOfConfig.h"
#include "BspCommUsart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 解析协议数据包
 * @param buffer 输入数据缓冲区
 *  @param bufferSize 输入数据缓冲区大小
 * @param packet 输出解析后的数据包结构体
 * @return ProtocolStatus_t 解析状态
 */
ProtocolStatus_t ProtocolParsePacket(const uint8_t* buffer, uint16_t bufferSize, ProtocolPacket_t* packet);
/**
 * @brief 计算CRC16校验值
 * @param data 输入数据缓冲区
 * @param length 输入数据长度
 * @return uint16_t CRC16校验值
 */
uint16_t Crc16Compute(const uint8_t* data, uint16_t length);
/**
 * @brief 验证CRC16校验值
 * @param data 输入数据缓冲区
 * @param length 输入数据长度
 * @param crc 输入的CRC16校验值
 * @return true 校验通过，false 校验失败
 */
bool ProtocolVerifyCrc(const uint8_t* data, uint16_t length, uint16_t crc);
/**
 * @brief 获取数据包的CRC校验值
 * @param buffer 输入数据缓冲区
 * @return uint16_t CRC校验值
 */
bool ProtocolCheckCRC(const uint8_t* buffer);
/**
 * @brief 创建包含子ID数据的数据包
 * @param Buffer 输出数据缓冲区
 * @param needAck 是否需要应答
 * @param mainId 主ID
 * @param subIds 子ID数据数组
 * @param count 子ID数量
 * @return uint16_t 创建的数据包长度
 */
uint16_t ProtocolCreateSubIdData(uint8_t *Buffer,uint16_t Head,bool needAck,uint8_t mainId, const uint8_t* subIds,uint8_t count);
/**
 * @brief 创建包含直接数据的数据包
 * @param Buffer 输出数据缓冲区
 * @param needAck 是否需要应答
 * @param mainId 主ID
 * @param data 直接数据缓冲区
 * @param dataSize 直接数据大小
 * @return uint16_t 创建的数据包长度
 */
uint16_t ProtocolCreateDirectData(uint8_t *Buffer,uint16_t Head,bool needAck,uint8_t mainId, const uint8_t* data,uint8_t dataSize);
/**
 * @brief 根据数据大小、有效性和缩放因子获取子ID长度字段
 * @param dataSize 数据大小
 * @param isValid 数据有效性
 * @param scale 缩放因子
 * @return uint8_t 子ID长度字段
 */
uint8_t ProtocolGetSubIdLength(uint8_t dataSize, bool isValid, uint8_t scale);
/**
 * @brief 获取数据包的帧头地址
 * @param buffer 输入数据缓冲区
 * @return uint16_t 帧头地址
 */
uint16_t ProtocolGetPacketHeader(const uint8_t* buffer);
/**
 * @brief 判断数据包是否需要应答
 * @param buffer 输入数据缓冲区
 * @return true 需要应答，false 不需要应答
 */
bool ProtocolIsAckRequired(const uint8_t* buffer);
/**
 * @brief 获取数据包的总大小
 * @param buffer 输入数据缓冲区
 * @return uint16_t 数据包总大小
 */
uint16_t ProtocolGetPacketSize(const uint8_t* buffer);
/**
 * @brief 获取数据包的CRC校验值
 * @param buffer 输入数据缓冲区
 * @return uint16_t CRC校验值
 */
uint16_t ProtocolGetCRC(const uint8_t* buffer);
/**
 * @brief 根据scale值返回实际的乘数
 * @param scale scale值
 * @return 实际乘数
 */
uint16_t ProtocolGetScale(uint8_t scale);
#ifdef __cplusplus
}
#endif

#endif /* _PROTOCOL_MCM_PACKETS_H_ */
