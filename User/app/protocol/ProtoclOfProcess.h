/**
**********************************************************************************
* @file     : ProtoclOfProcess.h
* @brief    : 通信协议调度和缓存处理头文件
* @details  : 实现通信的发送数据和接收数据调度，以及接收数据缓存处理，发送数据预处理
* @author   : \.
* @date     : 2025-01-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************
*/

#ifndef _PROTOCOL_PROCESS_H_
#define _PROTOCOL_PROCESS_H_

#include "ProtoclOfTypeDef.h"
#include "ProtoclOfConfig.h"
#include "ProtoclOfPackets.h"
#include "BspCommUsart.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 协议处理核心接口 ==================== */

/**
 * @brief 初始化协议处理模块
 * @param instance USART实例
 * @return ProtocolStatus_t 初始化状态
 */
ProtocolStatus_t ProtocolProcessInit(UsartInstance_t instance);

/**
 * @brief 反初始化协议处理模块
 */
void ProtocolProcessDeInit(void);

/**
 * @brief 协议主处理函数 - 在通信任务中调用
 * @param instance USART实例
 */
void ProtocolProcessMain(UsartInstance_t instance);

/**
 * @brief 协议调度处理函数
 * @param instance USART实例
 */
void ProtocolSchedulerProcess(UsartInstance_t instance);

/* ==================== 数据包接收函数 ==================== */
/**
 * @brief 接收数据处理函数
 * @param instance USART实例
 * @return ProtocolStatus_t 接收状态
 */
ProtocolStatus_t ProtocolReceiveData(UsartInstance_t instance);
/**
 * @brief 处理接收到的数据包
 * @param instance USART实例
 * @return ProtocolStatus_t 处理状态
 */
ProtocolStatus_t ProtocolProcessRxData(UsartInstance_t instance);
/**
 * @brief 注册应答包
 * @param instance USART实例
 * @param data 应答包数据
 */
void ProtocolRegisterAck(UsartInstance_t instance, const uint8_t* data, uint16_t size);
/**
 * @brief 处理应答超时检测和重发
 * @param instance USART实例
 */
void ProtocolProcessAckTimeout(UsartInstance_t instance);
/**
 * @brief 处理收到的应答包
 * @param instance USART实例
 * @param packet 收到的应答包
 */
void ProtocolProcessReceivedAck(UsartInstance_t instance, const ProtocolPacket_t* packet);
/* ==================== 发送数据预处理函数 ==================== */
/**
 * @brief 发送数据处理函数
 * @param instance USART实例
 * @param Priority 发送优先级
 * @param pData 指向发送数据的指针
 * @param length 发送数据的长度
 * @return ProtocolStatus_t 发送状态
 *  */
ProtocolStatus_t ProtocolSendData(UsartInstance_t instance, uint8_t Priority, uint8_t* pData, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* _PROTOCOL_MCM_PROCESS_H_ */
