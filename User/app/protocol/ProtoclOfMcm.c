/**
********************************************************************************
* @file     : ProtoclOfVcm.c
* @brief    : VCM相关的通信协议实现
* @details  : 该文件包含VCM相关的通信协议函数的实现
* @author   : \.
* @date     : 2025-01-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
*********************************************************************************
*/

#include "ProtoclOfMcm.h"
#include "breathscheduler.h"
#include "controldata.h"
#include "monitorengine.h"
#include "phasecontroller.h"
#include "rtos.h"
#include "log.h"
#include <math.h>


/* ==================== 静态变量定义 ==================== */
static uint16_t MCMConnectedCounter = 0;
/* 接收主ID缓存变量 */
static RxVentParamsCache_t g_rxVentParamsCache;
static RxVentSwitchCache_t g_rxVentSwitchCache;
static RxSystemMenuCache_t g_rxSystemMenuCache;
static RxManufacturerCache_t g_rxManufacturerCache;
static RxAlarmLimitsCache_t g_rxAlarmLimitsCache;
static RxSelfTestCache_t g_rxSelfTestCache;
static RxCalibrationCache_t g_rxCalibrationCache;
static RxDiagnosisCache_t g_rxDiagnosisCache;
// static RxDataQueryCache_t g_rxDataQueryCache; /* Pending machine port. */
// static RxOnlineUpgradeCache_t g_rxOnlineUpgradeCache; /* Pending machine port. */

/* 发送主ID缓存变量 */
// static TxWaveDataCache_t g_txWaveDataCache; /* Pending machine port. */
static TxMonitorParamsCache_t g_txMonitorParamsCache;
// static TxPhysAlarmCache_t g_txPhysAlarmCache; /* Pending machine port. */
// static TxTechAlarmCache_t g_txTechAlarmCache; /* Pending machine port. */
static TxSpecialFuncCache_t g_txSpecialFuncCache;
// static TxCalibDataCache_t g_txCalibDataCache; /* Pending machine port. */
// static TxDiagDataCache_t g_txDiagDataCache; /* Pending machine port. */
// static TxResultMsgCache_t g_txResultMsgCache; /* Pending machine port. */
// static TxErrorRequestCache_t g_txErrorRequestCache; /* Pending machine port. */
// static TxVersionInfoCache_t g_txVersionInfoCache; /* Pending machine port. */

static SelfCheckData_t SystemTest = {.curTest = 0xff};
static WaveData_t newWaveData;
static bool isMCMOnline = false;
static stProtocolHeartbeatStats gHeartbeatStats;

#if 0 /* Legacy machine self-test and flash hooks; pending port. */
static uint16_t ProtocolConvertSelfTestFloatToU16(float value, float scale)
{
    float scaledValue;

    if (value <= 0.0f) {
        return 0;
    }

    scaledValue = value * scale + 0.5f;
    if (scaledValue >= 65535.0f) {
        return UINT16_MAX;
    }

    return (uint16_t)scaledValue;
}



static bool ProtocolWriteUpgradeFlag(uint8_t flagCommand)
{
    FLASH_Status flashStatus;
    const volatile uint32_t* flagAddr = (const volatile uint32_t*)PROTOCOL_UPGRADE_FLAG_FLASH_ADDR;
    uint32_t flagValue;

    if (flagCommand == PROTOCOL_UPGRADE_FLAG_SET_VALUE) {
        flagValue = UPGRADE_CODE;
    } else if (flagCommand == PROTOCOL_UPGRADE_FLAG_CLEAR_VALUE) {
        flagValue = UPGRADE_FLAG_ERASE_VALUE;
    } else {
        return false;
    }

    if (*flagAddr == flagValue) {
        return true;
    }

    FLASH_Unlock();
    FLASH_ClearFlag(PROTOCOL_FLASH_CLEAR_ALL_FLAGS);

    flashStatus = FLASH_EraseSector(PROTOCOL_UPGRADE_FLAG_FLASH_SECTOR,
                                    PROTOCOL_UPGRADE_FLAG_FLASH_RANGE);
    if (flashStatus == FLASH_COMPLETE) {
        flashStatus = FLASH_ProgramWord(PROTOCOL_UPGRADE_FLAG_FLASH_ADDR, flagValue);
    }

    FLASH_Lock();

    return (flashStatus == FLASH_COMPLETE) && (*flagAddr == flagValue);
}
#endif
/* ==================== 数据包接收处理 ==================== */
void ProtocolProcessRxPacket(const ProtocolPacket_t* packet)
{
    if (!packet) {
        return;
    }
    MCMConnectedCounter = 0; // 收到数据包，重置断连计数器
    isMCMOnline = true; // 标记MCM在线
    repRtosEnterCritical();
    /* 根据主ID处理不同的数据包 */
    switch (packet->m_mainId) {
        case PROTOCOL_RX_MID_HEARTBEAT:
            /*心跳处理*/
            ++gHeartbeatStats.received;
            if (gHeartbeatStats.pending < UINT16_MAX) { ++gHeartbeatStats.pending; }
            else { ++gHeartbeatStats.overflow; }
            break;
        case PROTOCOL_RX_MID_VENT_PARAMS:
            /* 处理通气参数 - 更新到通气参数缓存 */
            ProtocolUpdateRxVentParamsCache(packet);
            protocolReceivedSettingsMark();
            break;
            
        case PROTOCOL_RX_MID_VENT_SWITCH:
            /* 处理通气开关 - 更新到通气开关缓存 */
            ProtocolUpdateRxVentSwitchCache(packet);
            break;
            
        case PROTOCOL_RX_MID_SYSTEM_MENU:
            /* 处理系统菜单 - 更新到系统菜单缓存 */
            ProtocolUpdateRxSystemMenuCache(packet);
            break;
            
        case PROTOCOL_RX_MID_ALARM_LIMITS:
            /* 处理报警限 - 更新到报警限缓存 */
            ProtocolUpdateRxAlarmLimitsCache(packet);
            protocolReceivedSettingsMark();
            break;
            
        case PROTOCOL_RX_MID_SELF_TEST:
            /* 处理自检 - 更新到自检缓存 */
            // ProtocolUpdateRxSelfTestCache(packet); /* Pending machine port. */
            break;
            
        case PROTOCOL_RX_MID_CALIBRATION:
            /* 处理校准 - 更新到校准缓存 */
            // ProtocolUpdateRxCalibrationCache(packet); /* Pending machine port. */
            break;
            
        case PROTOCOL_RX_MID_DIAGNOSIS:
            /* 处理诊断 - 更新到诊断缓存 */
            // ProtocolUpdateRxDiagnosisCache(packet); /* Pending machine port. */
            break;
            
        case PROTOCOL_RX_MID_DATA_QUERY:
            /* 处理数据查询 - 更新到数据查询缓存 */
            // ProtocolUpdateRxDataQueryCache(packet); /* Pending machine port. */
            break;
        case PROTOCOL_RX_MID_MANUFACTURER:
            ProtocolUpdateRxManufacturerCache(packet);
            break;
        case PROTOCOL_RX_MID_UPGRADE:
            /* 处理升级 */
            // ProtocolUpdateRxOnlineUpgradeCache(packet); /* Pending machine port. */
            break;
            
        default:
            break;
    }
    repRtosExitCritical();
}

/* ==================== 接收缓存更新函数 ==================== */
/**
 * @brief 更新通气参数缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxVentParamsCache(const ProtocolPacket_t* packet)
{
    eVentMode modeEnum = VENT_MD_IDLE;
    for (uint8_t i = 0; i < packet->m_subIdCount; i++) {
        const ProtocolSubId_t* subId = &packet->m_subIds[i];
        if (subId->m_isValid && subId->m_dataSize >= 1U && subId->m_dataSize <= 2U && subId->m_dataOffset + subId->m_dataSize <= packet->m_payloadSize && subId->m_subId < PROTOCOL_VENT_PARAMS_SUBID_MAX) {
            uint16_t value = 0;
            if (subId->m_dataSize == 1) {
                value = packet->m_payload[subId->m_dataOffset];
            } else if (subId->m_dataSize == 2) {
                value = packet->m_payload[subId->m_dataOffset] | 
                       (packet->m_payload[subId->m_dataOffset + 1] << 8);
            }
            
            /* 根据子ID更新对应的字段 */
            switch (subId->m_subId) {
                case 0x00: 
                    g_rxVentParamsCache.m_ventType = (uint8_t)value;
                    g_rxVentParamsCache.m_ventType_scale = subId->m_scale;
                    break;
                case 0x01: 
                    switch((uint8_t)value) {
                        case 0x00:  // P-A/C
                            modeEnum = VENT_MD_PAC;
                            break;
                        case 0x01:  // V-A/C
                            modeEnum = VENT_MD_VAC;
                            break;
                        case 0x02:  // P-SIMV
                            modeEnum = VENT_MD_P_SIMV;
                            break;
                        case 0x03:  // V-SIMV
                            modeEnum = VENT_MD_V_SIMV;
                            break;
                        case 0x04:  // CPAP/PSV
                            modeEnum = VENT_MD_CPAP_PSV;
                            break;
                        case 0x05:  // PRVC
                            modeEnum = VENT_MD_PRVC;
                            break;
                        case 0x06:  // PRVC-SIMV
                            modeEnum = VENT_MD_PRVC_SIMV;
                            break;
                        case 0x07:  // Dapap
                            modeEnum = VENT_MD_BAPAP;  // 注意：您的枚举是BAPAP
                            break;
                        case 0x08:  // CPRV
                            modeEnum = VENT_MD_CPRV;
                            break;
                        case 0x09:  // 预留
                            // 没有对应的枚举，使用默认值
                            modeEnum = VENT_MD_IDLE;
                            break;
                        case 0x0A:  // 氧疗
                            modeEnum = VENT_MD_HFO;
                            break;
                        case 0x0B:  // APRV
                            modeEnum = VENT_MD_APRV;
                            break;
                        case 0x0C:  // VS
                            modeEnum = VENT_MD_VS;
                            break;
                        case 0x0D:  // AMV
                            modeEnum = VENT_MD_AMV;
                            break;
                        case 0x0E:  // nCPAP
                            modeEnum = VENT_MD_NCPAP;
                            break;
                        case 0x0F:  // nCPAP-PC
                            modeEnum = VENT_MD_NCPAP_PC;
                            break;
                        case 0x10:  // PPS
                            modeEnum = VENT_MD_PPS;
                            break;
                        case 0x11:  // IAMV
                            modeEnum = VENT_MD_IAMV;
                            break;
                        case 0x12:  // PSV_ST
                            modeEnum = VENT_MD_PSV_ST;
                            break;
                        case 0x13:  // NIPPV
                            modeEnum = VENT_MD_NIPPV;
                            break;
                        case 0x14:  // VENT_MD_SNIPPV
                            modeEnum = VENT_MD_SNIPPV;
                            break;
                        default:
                            // 处理未知值
                            modeEnum = VENT_MD_IDLE;
                            break;
                    }   
                    g_rxVentParamsCache.m_modeRecv = modeEnum;
                    g_rxVentParamsCache.m_mode = g_rxVentParamsCache.m_modeRecv;
                    g_rxVentParamsCache.m_mode_scale = subId->m_scale;
                    break;
                
                case 0x02: 
                    g_rxVentParamsCache.m_patientType = (uint8_t)value;
                    g_rxVentParamsCache.m_patientType_scale = subId->m_scale;
                    break;
                case 0x03: 
                    g_rxVentParamsCache.m_gender = (uint8_t)value;
                    g_rxVentParamsCache.m_gender_scale = subId->m_scale;
                    break;
                case 0x04: 
                    g_rxVentParamsCache.m_idealHeight = (uint8_t)value;
                    g_rxVentParamsCache.m_idealHeight_scale = subId->m_scale;
                    break;
                case 0x05: 
                    g_rxVentParamsCache.m_idealWeight = value;
                    g_rxVentParamsCache.m_idealWeight_scale = subId->m_scale;
                    break;
                case 0x06: 
                    g_rxVentParamsCache.m_fio2 = (uint8_t)value;
                    g_rxVentParamsCache.m_fio2_scale = subId->m_scale;
                    break;
                case 0x07: 
                    g_rxVentParamsCache.m_deltaPinsp = (uint8_t)value;
                    g_rxVentParamsCache.m_deltaPinsp_scale = subId->m_scale;
                    break;
                case 0x08: 
                    g_rxVentParamsCache.m_peep = (uint8_t)value;
                    g_rxVentParamsCache.m_peep_scale = subId->m_scale;
                    break;
                case 0x09: 
                    g_rxVentParamsCache.m_deltaPsupp = (uint8_t)value;
                    g_rxVentParamsCache.m_deltaPsupp_scale = subId->m_scale;
                    break;
                case 0x0A: 
                    g_rxVentParamsCache.m_pHigh = (uint8_t)value;
                    g_rxVentParamsCache.m_pHigh_scale = subId->m_scale;
                    break;
                case 0x0B: 
                    g_rxVentParamsCache.m_pLow = (uint8_t)value;
                    g_rxVentParamsCache.m_pLow_scale = subId->m_scale;
                    break;
                case 0x0C: 
                    g_rxVentParamsCache.m_deltaApneaP = (uint8_t)value;
                    g_rxVentParamsCache.m_deltaApneaP_scale = subId->m_scale;
                    break;
                case 0x0D: 
                    g_rxVentParamsCache.m_deltaSighP = (uint8_t)value;
                    g_rxVentParamsCache.m_deltaSighP_scale = subId->m_scale;
                    break;
                case 0x0E: 
                    g_rxVentParamsCache.m_tidalVolume = value;
                    g_rxVentParamsCache.m_tidalVolume_scale = subId->m_scale;
                    break;
                case 0x0F: 
                    g_rxVentParamsCache.m_apneaTidalVolume = value;
                    g_rxVentParamsCache.m_apneaTidalVolume_scale = subId->m_scale;
                    break;
                case 0x10: 
                    g_rxVentParamsCache.m_flow = (uint16_t)value;
                    g_rxVentParamsCache.m_flow_scale = subId->m_scale;
                    break;
                case 0x11: 
                    g_rxVentParamsCache.m_trigFlow = value;
                    g_rxVentParamsCache.m_trigFlow_scale = subId->m_scale;
                    break;
                case 0x12: 
                    g_rxVentParamsCache.m_trigPress = (int16_t)value;
                    g_rxVentParamsCache.m_trigPress_scale = subId->m_scale;
                    break;
                case 0x13: 
                    g_rxVentParamsCache.m_exhTrigPercent = (uint8_t)value;
                    g_rxVentParamsCache.m_exhTrigPercent_scale = subId->m_scale;
                    break;
                case 0x14: 
                    g_rxVentParamsCache.m_rate = (uint8_t)value;
                    g_rxVentParamsCache.m_rate_scale = subId->m_scale;
                    break;
                case 0x15: 
                    g_rxVentParamsCache.m_simvRate = (uint8_t)value;
                    g_rxVentParamsCache.m_simvRate_scale = subId->m_scale;
                    break;
                case 0x16: 
                    g_rxVentParamsCache.m_apneaRate = (uint8_t)value;
                    g_rxVentParamsCache.m_apneaRate_scale = subId->m_scale;
                    break;
                case 0x17: 
                    g_rxVentParamsCache.m_ti = value;
                    g_rxVentParamsCache.m_ti_scale = subId->m_scale;
                    break;
                case 0x18: 
                    g_rxVentParamsCache.m_tiMax = value;
                    g_rxVentParamsCache.m_tiMax_scale = subId->m_scale;
                    break;
                case 0x19: 
                    g_rxVentParamsCache.m_apneaTi = value;
                    g_rxVentParamsCache.m_apneaTi_scale = subId->m_scale;
                    break;
                case 0x1A: 
                    g_rxVentParamsCache.m_riseTime = value;
                    g_rxVentParamsCache.m_riseTime_scale = subId->m_scale;
                    break;
                case 0x1B: 
                    g_rxVentParamsCache.m_tHigh = value;
                    g_rxVentParamsCache.m_tHigh_scale = subId->m_scale;
                    break;
                case 0x1C: 
                    g_rxVentParamsCache.m_tLow = value;
                    g_rxVentParamsCache.m_tLow_scale = subId->m_scale;
                    break;
                case 0x1D: 
                    g_rxVentParamsCache.m_sighCount = (uint8_t)value;
                    g_rxVentParamsCache.m_sighCount_scale = subId->m_scale;
                    break;
                case 0x1E: 
                    g_rxVentParamsCache.m_sighIntervalMin = (uint16_t)value;
                    g_rxVentParamsCache.m_sighIntervalMin_scale = subId->m_scale;
                    break;
                case 0x1F: 
                    g_rxVentParamsCache.m_siPress = (uint8_t)value;
                    g_rxVentParamsCache.m_siPress_scale = subId->m_scale;
                    break;
                case 0x20: 
                    g_rxVentParamsCache.m_siTime = (uint8_t)value;
                    g_rxVentParamsCache.m_siTime_scale = subId->m_scale;
                    break;
                case 0x21: 
                    g_rxVentParamsCache.m_sbtDuration = (uint8_t)value;
                    g_rxVentParamsCache.m_sbtDuration_scale = subId->m_scale;
                    break;
                case 0x22: 
                    g_rxVentParamsCache.m_sbtTolerance = (uint8_t)value;
                    g_rxVentParamsCache.m_sbtTolerance_scale = subId->m_scale;
                    break;
                case 0x23:  // 0x23 - 呼气时间
                    g_rxVentParamsCache.m_expTime = (uint8_t)value;
                    g_rxVentParamsCache.m_expTime_scale = subId->m_scale;
                    break;
                case 0x24:  // 0x24 - 辅助触发开关
                    g_rxVentParamsCache.m_assistTrig = (uint8_t)value;
                    g_rxVentParamsCache.m_assistTrig_scale = subId->m_scale;
                    break;
                case 0x25:  // 0x25 - 流速触发/压力触发
                    g_rxVentParamsCache.m_FlowTrigger = (uint8_t)value;
                    g_rxVentParamsCache.m_FlowTrigger_scale = subId->m_scale;
                    break;
                case 0x26:  // 0x26 - 吸气暂停百分比
                    g_rxVentParamsCache.m_inspPausePercent = (uint8_t)value;
                    g_rxVentParamsCache.m_inspPausePercent_scale = subId->m_scale;
                    break;
                case 0x27:  // 0x27 - 经鼻持续气道正压 (nCPAP)
                    g_rxVentParamsCache.m_nCPAP = (uint16_t)value;
                    g_rxVentParamsCache.m_nCPAP_scale = subId->m_scale;
                    break;
                case 0x28:  // 0x28 - 经鼻间歇气道正压 (nIPAP)
                    g_rxVentParamsCache.m_nIPAP = (uint16_t)value;
                    g_rxVentParamsCache.m_nIPAP_scale = subId->m_scale;
                    break;
                case 0x29:  // 0x29 - 最高压
                    g_rxVentParamsCache.m_peakPressure = (uint8_t)value;
                    g_rxVentParamsCache.m_peakPressure_scale = subId->m_scale;
                    break;
                case 0x2A:  // 0x2A - 最大潮气量 (成人)
                    g_rxVentParamsCache.m_maxTidalVolume = (uint16_t)value;
                    g_rxVentParamsCache.m_maxTidalVolume_scale = subId->m_scale;
                    break;
                case 0x2B:  // 0x2B - 最大容辅
                    g_rxVentParamsCache.m_maxVolumeAssist = (uint16_t)value;
                    g_rxVentParamsCache.m_maxVolumeAssist_scale = subId->m_scale;
                    break;
                case 0x2C:  // 0x2C - 最大流辅
                    g_rxVentParamsCache.m_maxFlowAssist = (uint8_t)value;
                    g_rxVentParamsCache.m_maxFlowAssist_scale = subId->m_scale;
                    break;
                case 0x2D:  // 0x2D - 呼吸支持%
                    g_rxVentParamsCache.m_breathSupportPercent = (uint8_t)value;
                    g_rxVentParamsCache.m_breathSupportPercent_scale = subId->m_scale;
                    break;
                case 0x2E:  // 0x2E - 分钟通气量%
                    g_rxVentParamsCache.m_minuteVentilationPercent = (uint16_t)value;
                    g_rxVentParamsCache.m_minuteVentilationPercent_scale = subId->m_scale;
                    break;
                case 0x2F:  // 0x2F - Pstart
                    g_rxVentParamsCache.m_pStart = (uint8_t)value;  // 修正为Pstart
                    g_rxVentParamsCache.m_pStart_scale = subId->m_scale;
                    break;
                case 0x30:  // 0x30 - Pmax
                    g_rxVentParamsCache.m_pMax = (uint8_t)value;  // 修正为Pmax
                    g_rxVentParamsCache.m_pMax_scale = subId->m_scale;
                    break;
                case 0x31:  // 0x31 - 氧疗流速
                    g_rxVentParamsCache.m_o2TherapyFlow  = (uint16_t)value;
                    g_rxVentParamsCache.m_o2TherapyFlow_scale = subId->m_scale;
                    break;
                case 0x32:  // 0x32 - PEEPend
                    g_rxVentParamsCache.m_peepEnd = (uint8_t)value;
                    g_rxVentParamsCache.m_peepEnd_scale = subId->m_scale;
                    break;
                case 0x33:  // 0x33 - Ramp
                    g_rxVentParamsCache.m_ramp = (uint8_t)value;
                    g_rxVentParamsCache.m_ramp_scale = subId->m_scale;
                    break;
                case 0x34:  // 0x34 - Tpause
                    g_rxVentParamsCache.m_tPause = (uint8_t)value;
                    g_rxVentParamsCache.m_tPause_scale = subId->m_scale;
                    break;
                case 0x35:  // 0x35 - Tmaneuver
                    g_rxVentParamsCache.m_tManeuver = (uint8_t)value;
                    g_rxVentParamsCache.m_tManeuver_scale = subId->m_scale;
                    break;
                case 0x36:  // 0x36 - Thold
                    g_rxVentParamsCache.m_tHold = (uint8_t)value;
                    g_rxVentParamsCache.m_tHold_scale = subId->m_scale;
                    break;
                case 0x37:  // 0x37 - Phold
                    g_rxVentParamsCache.m_pHold = (uint8_t)value;
                    g_rxVentParamsCache.m_pHold_scale = subId->m_scale;
                    break;
                case 0x38:  // 0x38 - 插管类型
                    g_rxVentParamsCache.m_intub_type = (uint8_t)value;
                    g_rxVentParamsCache.m_intub_type_scale = subId->m_scale;
                    break;
                case 0x39:  // 0x39 - 管径
                    g_rxVentParamsCache.m_tube_diam = (uint8_t)value;
                    g_rxVentParamsCache.m_tube_diam_scale = subId->m_scale;
                    break;
                case 0x40:  // 0x40 - 插管补偿比例
                    g_rxVentParamsCache.m_comp_ratio = (uint8_t)value;
                    g_rxVentParamsCache.m_comp_ratio_scale = subId->m_scale;
                    break;
                case 0x41:  // 0x41 - 自动插管阻力补偿
                    g_rxVentParamsCache.m_auto_irc = (uint8_t)value;
                    break;
                case 0x42:  // 0x42 - 插管呼气阶段补偿
                    g_rxVentParamsCache.m_exp_comp = (uint8_t)value;
                    break;
                default:
                    break;
            }
            g_rxVentParamsCache.m_valid[subId->m_subId] = true;

        }
    }  
}


/**
 * @brief 更新通气开关缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxVentSwitchCache(const ProtocolPacket_t* packet)
{
    for (uint8_t i = 0; i < packet->m_subIdCount; i++) {
        const ProtocolSubId_t* subId = &packet->m_subIds[i];
        if (subId->m_isValid && subId->m_dataSize == 1U && subId->m_dataOffset + 1U <= packet->m_payloadSize && subId->m_subId < PROTOCOL_VENT_SWITCH_SUBID_MAX) {
            uint8_t value = packet->m_payload[subId->m_dataOffset];
            
            switch (subId->m_subId) {
                case 0x00: 
                    if (value > 1U) { break; }
                    protocolReceivedCommandMark();
                    g_rxVentSwitchCache.m_command = value;
                    g_rxVentSwitchCache.m_command_scale = subId->m_scale;
                    if(g_rxVentSwitchCache.m_command == 0x00) {
                        g_rxVentParamsCache.m_mode = VENT_MD_IDLE;
                    } else if(g_rxVentSwitchCache.m_command == 0x01) {
                        g_rxVentParamsCache.m_mode = g_rxVentParamsCache.m_modeRecv;
                    }
                    break;
                case 0x01: 
                    g_rxVentSwitchCache.m_sigh = value;
                    g_rxVentSwitchCache.m_sigh_scale = subId->m_scale;
                    break;
                case 0x02: 
                    g_rxVentSwitchCache.m_syncEnhance = value;
                    g_rxVentSwitchCache.m_syncEnhance_scale = subId->m_scale;
                    break;
                case 0x03: 
                    g_rxVentSwitchCache.m_manualBreath = value;
                    g_rxVentSwitchCache.m_manualBreath_scale = subId->m_scale;
                    break;
                case 0x04: 
                    g_rxVentSwitchCache.m_inspHold = value;
                    g_rxVentSwitchCache.m_inspHold_scale = subId->m_scale;
                    break;
                case 0x05: 
                    g_rxVentSwitchCache.m_exhHold = value;
                    g_rxVentSwitchCache.m_exhHold_scale = subId->m_scale;
                    break;
                case 0x06: 
                    g_rxVentSwitchCache.m_o2Boost = value;
                    g_rxVentSwitchCache.m_o2Boost_scale = subId->m_scale;
                    break;
                case 0x07: 
                    g_rxVentSwitchCache.m_o2Boost2 = value;
                    g_rxVentSwitchCache.m_o2Boost2_scale = subId->m_scale;
                    break;
                case 0x08: 
                    g_rxVentSwitchCache.m_si = value;
                    g_rxVentSwitchCache.m_si_scale = subId->m_scale;
                    break;
                case 0x09: 
                    g_rxVentSwitchCache.m_sbt = value;
                    g_rxVentSwitchCache.m_sbt_scale = subId->m_scale;
                    break;
                case 0x0A:
                    g_rxVentSwitchCache.m_ApneaVentSwitch = value;
                    g_rxVentSwitchCache.m_ApneaVentSwitch_scale = subId->m_scale;
                    break;
                case 0x0B:  /* P0.1 (吸气压力阈值) */
                    g_rxVentSwitchCache.m_P01 = value;
                    g_rxVentSwitchCache.m_P01_scale = subId->m_scale;
                    break;
                
                case 0x0C:  /* NIF (最大吸气负压) */
                    g_rxVentSwitchCache.m_nif = value;
                    g_rxVentSwitchCache.m_nif_scale = subId->m_scale;
                    break;
                
                case 0x0D:  /* PEEPi (内源性PEEP) */
                    g_rxVentSwitchCache.m_peepi = value;
                    g_rxVentSwitchCache.m_peepi_scale = subId->m_scale;
                    break;
                
                case 0x0E:  /* P-V (压力-容积曲线) */
                    g_rxVentSwitchCache.m_p_v = value;
                    g_rxVentSwitchCache.m_p_v_scale = subId->m_scale;
                    break;

                case 0x0F:
                    g_rxVentSwitchCache.m_cprVent = value;
                    g_rxVentSwitchCache.m_cprVent_scale = subId->m_scale;
                    g_txSpecialFuncCache.m_cprVent = value;
                    g_txSpecialFuncCache.m_valid[0x0D] = true;
                    break;
                
                default:
                    /* 未知地址处理 */
                    break;
            }
            g_rxVentSwitchCache.m_valid[subId->m_subId] = true;
        }
    }
}


/**
 * @brief 更新系统菜单缓存
 * @param packet 接收到的数据包 
 * */   
void ProtocolUpdateRxSystemMenuCache(const ProtocolPacket_t* packet)
{
    for (uint8_t i = 0; i < packet->m_subIdCount; i++) {
        const ProtocolSubId_t* subId = &packet->m_subIds[i];
        if (subId->m_isValid && subId->m_subId < PROTOCOL_SYSTEM_MENU_SUBID_MAX) {
            uint16_t value = 0;
            if (subId->m_dataSize == 1) {
                value = packet->m_payload[subId->m_dataOffset];
            } else if (subId->m_dataSize == 2) {
                value = packet->m_payload[subId->m_dataOffset] | 
                    (packet->m_payload[subId->m_dataOffset + 1] << 8);
            }
            
            /* 根据子ID更新对应的系统菜单字段 */
            switch (subId->m_subId) {
                case 0x00: 
                    g_rxSystemMenuCache.m_ibwTidalVolume = value;
                    g_rxSystemMenuCache.m_ibwTidalVolume_scale = subId->m_scale;
                    break;
                case 0x01: 
                    g_rxSystemMenuCache.m_o2BoostFio2 = (uint8_t)value;
                    g_rxSystemMenuCache.m_o2BoostFio2_scale = subId->m_scale;
                    break;
                case 0x02: 
                    g_rxSystemMenuCache.m_o2BoostTime = (uint8_t)value;
                    g_rxSystemMenuCache.m_o2BoostTime_scale = subId->m_scale;
                    break;
                case 0x03: 
                    g_rxSystemMenuCache.m_suctionTime = (uint8_t)value;
                    g_rxSystemMenuCache.m_suctionTime_scale = subId->m_scale;
                    break;
                case 0x04: 
                    g_rxSystemMenuCache.m_o2SourceType = (uint8_t)value;
                    g_rxSystemMenuCache.m_o2SourceType_scale = subId->m_scale;
                    break;
                case 0x05: 
                    g_rxSystemMenuCache.m_altitude = (uint8_t)value;
                    g_rxSystemMenuCache.m_altitude_scale = subId->m_scale;
                    break;
                case 0x06: 
                    g_rxSystemMenuCache.m_gasVolumeStd = (uint8_t)value;
                    g_rxSystemMenuCache.m_gasVolumeStd_scale = subId->m_scale;
                    break;
                case 0x07: 
                    g_rxSystemMenuCache.m_o2SensorSwitch = (uint8_t)value;
                    g_rxSystemMenuCache.m_o2SensorSwitch_scale = subId->m_scale;
                    break;
                case 0x08: 
                    g_rxSystemMenuCache.m_ApneaVentType = (uint8_t)value;
                    g_rxSystemMenuCache.m_ApneaVentType_scale = subId->m_scale;
                    break;
                case 0x09: 
                    g_rxSystemMenuCache.m_InspType = (uint8_t)value;
                    g_rxSystemMenuCache.m_InspType_scale = subId->m_scale;
                    break;
                case 0x0A:
                    g_rxSystemMenuCache.m_LeakageComp = (uint8_t)value;
                    g_rxSystemMenuCache.m_LeakageComp_scale = subId->m_scale;
                    break;
            }
            g_rxSystemMenuCache.m_valid[subId->m_subId] = true;
        }
    }
}

/**
 * @brief Update manufacturer config cache
 * @param packet received packet
 */
void ProtocolUpdateRxManufacturerCache(const ProtocolPacket_t* packet)
{
    for (uint8_t i = 0; i < packet->m_subIdCount; i++) {
        const ProtocolSubId_t* subId = &packet->m_subIds[i];
        if (subId->m_isValid && subId->m_subId < PROTOCOL_MANUFACTURER_SUBID_MAX) {
            uint16_t value = 0;
            if (subId->m_dataSize == 1) {
                value = packet->m_payload[subId->m_dataOffset];
            } else if (subId->m_dataSize == 2) {
                value = packet->m_payload[subId->m_dataOffset] |
                    (packet->m_payload[subId->m_dataOffset + 1] << 8);
            } else {
                continue;
            }

            switch (subId->m_subId) {
                case 0x00:
                    g_rxManufacturerCache.m_gasCorrectMode = (uint8_t)value;
                    g_rxManufacturerCache.m_gasCorrectMode_scale = subId->m_scale;
                    break;
                case 0x01:
                    g_rxManufacturerCache.m_altitude = value;
                    g_rxManufacturerCache.m_altitude_scale = subId->m_scale;
                    break;
                default:
                    break;
            }
            g_rxManufacturerCache.m_valid[subId->m_subId] = true;
        }
    }
}

/**
 * @brief Update alarm limits cache
 * @param packet received packet
 */
void ProtocolUpdateRxAlarmLimitsCache(const ProtocolPacket_t* packet)
{
    for (uint8_t i = 0; i < packet->m_subIdCount; i++) {
        const ProtocolSubId_t* subId = &packet->m_subIds[i];
        if (subId->m_isValid && subId->m_dataOffset + subId->m_dataSize <= packet->m_payloadSize && subId->m_subId < PROTOCOL_ALARM_LIMITS_SUBID_MAX) {
            uint16_t value = 0;
            if (subId->m_dataSize == 1) {
                value = packet->m_payload[subId->m_dataOffset];
            } else if (subId->m_dataSize == 2) {
                value = packet->m_payload[subId->m_dataOffset] | 
                       (packet->m_payload[subId->m_dataOffset + 1] << 8);
            } else {
                continue;
            }
            
            /* 根据子ID更新对应的报警限字段 */
            switch (subId->m_subId) {
                case 0x00: 
                    g_rxAlarmLimitsCache.m_pAirwayLow = value;
                    g_rxAlarmLimitsCache.m_pAirwayLow_scale = subId->m_scale;
                    break;
                case 0x01: 
                    g_rxAlarmLimitsCache.m_pAirwayHigh = value;
                    g_rxAlarmLimitsCache.m_pAirwayHigh_scale = subId->m_scale;
                    break;
                case 0x02: 
                    g_rxAlarmLimitsCache.m_mvHigh = value;
                    g_rxAlarmLimitsCache.m_mvHigh_scale = subId->m_scale;
                    break;
                case 0x03: 
                    g_rxAlarmLimitsCache.m_mvLow = value;
                    g_rxAlarmLimitsCache.m_mvLow_scale = subId->m_scale;
                    break;
                case 0x04: 
                    g_rxAlarmLimitsCache.m_tveHigh = value;
                    g_rxAlarmLimitsCache.m_tveHigh_scale = subId->m_scale;
                    break;
                case 0x05: 
                    g_rxAlarmLimitsCache.m_tveLow = value;
                    g_rxAlarmLimitsCache.m_tveLow_scale = subId->m_scale;
                    break;
                case 0x06: 
                    g_rxAlarmLimitsCache.m_fio2High = (uint8_t)value;
                    g_rxAlarmLimitsCache.m_fio2High_scale = subId->m_scale;
                    break;
                case 0x07: 
                    g_rxAlarmLimitsCache.m_fio2Low = (uint8_t)value;
                    g_rxAlarmLimitsCache.m_fio2Low_scale = subId->m_scale;
                    break;
                case 0x08: 
                    g_rxAlarmLimitsCache.m_frTotalHigh = (uint8_t)value;
                    g_rxAlarmLimitsCache.m_frTotalHigh_scale = subId->m_scale;
                    break;
                case 0x09: 
                    g_rxAlarmLimitsCache.m_frTotalLow = (uint8_t)value;
                    g_rxAlarmLimitsCache.m_frTotalLow_scale = subId->m_scale;
                    break;
                case 0x0A: 
                    g_rxAlarmLimitsCache.m_apneaTime = (uint8_t)value;
                    g_rxAlarmLimitsCache.m_apneaTime_scale = subId->m_scale;
                    break;
            }
            g_rxAlarmLimitsCache.m_valid[subId->m_subId] = true;
        }
    }
}

/**
 * @brief 更新自检缓存
 * @param packet 接收到的数据包
 */
#if 0 /* Legacy machine features retained for later migration. */
void ProtocolUpdateRxSelfTestCache(const ProtocolPacket_t* packet)
{
    for (uint8_t i = 0; i < packet->m_subIdCount; i++) {
        const ProtocolSubId_t* subId = &packet->m_subIds[i];
        if (subId->m_isValid && subId->m_subId < PROTOCOL_SELF_TEST_SUBID_MAX) {  // 更新为12个项目
            uint16_t value = 0;
            if (subId->m_dataSize == 1) {
                value = packet->m_payload[subId->m_dataOffset];
            } else if (subId->m_dataSize == 2) {
                value = packet->m_payload[subId->m_dataOffset] | 
                       (packet->m_payload[subId->m_dataOffset + 1] << 8);
            }
            
            /* 根据子ID更新对应的自检字段 */
            switch (subId->m_subId) {
                case 0x00: 
                    g_rxSelfTestCache.m_system = (uint8_t)value;
                    g_rxSelfTestCache.m_system_scale = subId->m_scale;
                    break;
                case 0x01: 
                    g_rxSelfTestCache.m_turbine = (uint8_t)value;
                    g_rxSelfTestCache.m_turbine_scale = subId->m_scale;
                    SystemTest.curTest = subId->m_subId-1;
                    SystemTest.isInProgress = true;
                    SystemTest.status = (uint8_t)value;
                    break;
                case 0x02: 
                    g_rxSelfTestCache.m_o2FlowSensor = (uint8_t)value;
                    g_rxSelfTestCache.m_o2FlowSensor_scale = subId->m_scale;
                    SystemTest.curTest = subId->m_subId-1;
                    SystemTest.isInProgress = true;
                    SystemTest.status = (uint8_t)value;
                    break;
                case 0x03: 
                    g_rxSelfTestCache.m_inspFlowSensor = (uint8_t)value;
                    g_rxSelfTestCache.m_inspFlowSensor_scale = subId->m_scale;
                    SystemTest.curTest = subId->m_subId-1;
                    SystemTest.isInProgress = true;
                    SystemTest.status = (uint8_t)value;
                    break;
                case 0x04: 
                    g_rxSelfTestCache.m_pressureSensor = (uint8_t)value;
                    g_rxSelfTestCache.m_pressureSensor_scale = subId->m_scale;
                    SystemTest.curTest = subId->m_subId-1;
                    SystemTest.isInProgress = true;
                    SystemTest.status = (uint8_t)value;
                    break;
                case 0x05: 
                    g_rxSelfTestCache.m_expValve = (uint8_t)value;
                    g_rxSelfTestCache.m_expValve_scale = subId->m_scale;
                    SystemTest.curTest = subId->m_subId-1;
                    SystemTest.isInProgress = true;
                    SystemTest.status = (uint8_t)value;
                    break;
                case 0x06: 
                    g_rxSelfTestCache.m_safetyValve = (uint8_t)value;
                    g_rxSelfTestCache.m_safetyValve_scale = subId->m_scale;
                    SystemTest.curTest = subId->m_subId-1;
                    SystemTest.isInProgress = true;
                    SystemTest.status = (uint8_t)value;
                    break;
                case 0x07: 
                    g_rxSelfTestCache.m_leakTest = (uint8_t)value;
                    g_rxSelfTestCache.m_leakTest_scale = subId->m_scale;
                    SystemTest.curTest = subId->m_subId-1;
                    SystemTest.isInProgress = true;
                    SystemTest.status = (uint8_t)value;
                    break;
                case 0x08: 
                    g_rxSelfTestCache.m_complianceTest = (uint8_t)value;
                    g_rxSelfTestCache.m_complianceTest_scale = subId->m_scale;
                    SystemTest.curTest = subId->m_subId-1;
                    SystemTest.isInProgress = true;
                    SystemTest.status = (uint8_t)value;
                    break;
                case 0x09: 
                    g_rxSelfTestCache.m_tubeResistanceTest = (uint8_t)value;
                    g_rxSelfTestCache.m_tubeResistanceTest_scale = subId->m_scale;
                    SystemTest.curTest = subId->m_subId-1;
                    SystemTest.isInProgress = true;
                    SystemTest.status = (uint8_t)value;
                    break;
                case 0x0A: 
                    g_rxSelfTestCache.m_proxFlowSensor = (uint8_t)value;
                    g_rxSelfTestCache.m_proxFlowSensor_scale = subId->m_scale;
                    SystemTest.curTest = subId->m_subId-1;
                    SystemTest.isInProgress = true;
                    SystemTest.status = (uint8_t)value;
                    break;
                case 0x0B: 
                    g_rxSelfTestCache.m_o2SensorTest = (uint8_t)value;
                    g_rxSelfTestCache.m_o2SensorTest_scale = subId->m_scale;
                    SystemTest.curTest = subId->m_subId-1;
                    SystemTest.isInProgress = true;
                    SystemTest.status = (uint8_t)value;
                    break;
            }
            g_rxSelfTestCache.m_valid[subId->m_subId] = true;
        }
    }
}

/**
 * @brief 更新校准缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxCalibrationCache(const ProtocolPacket_t* packet)
{
    MCalibFunMgrRxCache_t *calibCache = MCalibFunMgrGetRxCache();
    for (uint8_t i = 0; i < packet->m_subIdCount; i++) {
        const ProtocolSubId_t* subId = &packet->m_subIds[i];
        /* 支持校准项目0x00~0x06及独立的管路选择0x07 */
        if (subId->m_isValid && subId->m_subId < PROTOCOL_CALIBRATION_SUBID_MAX) {
            uint16_t value = 0;
            if (subId->m_dataSize == 1) {
                value = packet->m_payload[subId->m_dataOffset];
            } else if (subId->m_dataSize == 2) {
                value = packet->m_payload[subId->m_dataOffset] | 
                       (packet->m_payload[subId->m_dataOffset + 1] << 8);
            }
            
            /* 根据子ID更新对应的校准字段 */
            switch (subId->m_subId) {
                case 0x00: 
                    g_rxCalibrationCache.m_zeroCalib = (uint8_t)value;
                    g_rxCalibrationCache.m_zeroCalib_scale = subId->m_scale;
                    break;
                case 0x01: 
                    g_rxCalibrationCache.m_pressureCalib = (uint8_t)value;
                    g_rxCalibrationCache.m_pressureCalib_scale = subId->m_scale;
                    break;
                case 0x02:
                    g_rxCalibrationCache.m_proxFlowSensorCalib = (uint8_t)value;
                    g_rxCalibrationCache.m_proxFlowSensorCalib_scale = subId->m_scale;
                    break;
                case 0x03:
                    g_rxCalibrationCache.m_o2RatioValveCalib = (uint8_t)value;
                    g_rxCalibrationCache.m_o2RatioValveCalib_scale = subId->m_scale;
                    break;
                
                case 0x04:
                    g_rxCalibrationCache.m_airO2MixCoeffCalib = (uint8_t)value;
                    g_rxCalibrationCache.m_airO2MixCoeffCalib_scale = subId->m_scale;
                    break;
                case 0x05:
                    g_rxCalibrationCache.m_o2SensorCalib = (uint8_t)value;
                    g_rxCalibrationCache.m_o2SensorCalib_scale = subId->m_scale;
                    break;
                case 0x06:
                    g_rxCalibrationCache.m_expValveCalib = (uint8_t)value;
                    g_rxCalibrationCache.m_expValveCalib_scale = subId->m_scale;
                    break;
                case 0x07:
                    g_rxCalibrationCache.m_flowCalibPipe = (uint8_t)value;
                    g_rxCalibrationCache.m_flowCalibPipe_scale = subId->m_scale;
                    CalibFuncDiffFlowSetPipeType((uint8_t)value);
                    break;
            }
            g_rxCalibrationCache.m_valid[subId->m_subId] = true;
            
            /* 只有0x00~0x06是校准项目，0x07仅用于选择流量校准管路 */
            if(subId->m_subId != 0x07) {
                calibCache->currentState = (CalibrationType)subId->m_subId;
                calibCache->commandData = value;
                calibCache->isValid = true;
            }
        }
    }
}

/**
 * @brief 更新诊断缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxDiagnosisCache(const ProtocolPacket_t* packet)
{
    for (uint8_t i = 0; i < packet->m_subIdCount; i++) {
        const ProtocolSubId_t* subId = &packet->m_subIds[i];
        if (subId->m_isValid && subId->m_subId < PROTOCOL_DIAGNOSIS_SUBID_MAX) {
            uint16_t value = 0;
            if (subId->m_dataSize == 1) {
                value = packet->m_payload[subId->m_dataOffset];
            } else if (subId->m_dataSize == 2) {
                value = packet->m_payload[subId->m_dataOffset] | 
                       (packet->m_payload[subId->m_dataOffset + 1] << 8);
            }
            
            /* 根据子ID更新对应的诊断字段 */
            switch (subId->m_subId) {
                case 0x00: 
                    g_rxDiagnosisCache.m_diagnosisSwitch = (uint8_t)value;
                    g_rxDiagnosisCache.m_diagnosisSwitch_scale = subId->m_scale;
                    break;
                case 0x01: 
                    g_rxDiagnosisCache.m_inspZeroValveCtrl = (uint8_t)value;
                    g_rxDiagnosisCache.m_inspZeroValveCtrl_scale = subId->m_scale;
                    break;  
                case 0x02: 
                    g_rxDiagnosisCache.m_proxPressureZero = (uint8_t)value;
                    g_rxDiagnosisCache.m_proxPressureZero_scale = subId->m_scale;
                    break;  
                case 0x03: 
                    g_rxDiagnosisCache.m_proxFlowZero = (uint8_t)value;
                    g_rxDiagnosisCache.m_proxFlowZero_scale = subId->m_scale;
                    break;
                case 0x04: 
                    g_rxDiagnosisCache.m_flushValveCtrl = (uint8_t)value;
                    g_rxDiagnosisCache.m_flushValveCtrl_scale = subId->m_scale;
                    break;
                case 0x05: 
                    g_rxDiagnosisCache.m_safetyValveCtrl = (uint8_t)value;
                    g_rxDiagnosisCache.m_safetyValveCtrl_scale = subId->m_scale;
                    break;
                case 0x06: 
                    g_rxDiagnosisCache.m_turbinePressureCtrl = (uint8_t)value;
                    g_rxDiagnosisCache.m_turbinePressureCtrl_scale = subId->m_scale;
                    break;
                case 0x07: 
                    g_rxDiagnosisCache.m_turbineSpeedCtrl = (uint16_t)value;
                    g_rxDiagnosisCache.m_turbineSpeedCtrl_scale = subId->m_scale;
                    break;
                case 0x08: 
                    g_rxDiagnosisCache.m_turbineFlowCtrl = (uint16_t)value;
                    g_rxDiagnosisCache.m_turbineFlowCtrl_scale = subId->m_scale;
                    break;             
                case 0x09: 
                    g_rxDiagnosisCache.m_turbineDutyCycle = (uint8_t)value;
                    g_rxDiagnosisCache.m_turbineDutyCycle_scale = subId->m_scale;
                    break;
                case 0x0A: 
                    g_rxDiagnosisCache.m_o2ValveFlowCtrl = (uint8_t)value;
                    g_rxDiagnosisCache.m_o2ValveFlowCtrl_scale = subId->m_scale;
                    break;
                case 0x0B: 
                    g_rxDiagnosisCache.m_o2ValveCurrent = (uint16_t)value;
                    g_rxDiagnosisCache.m_o2ValveCurrent_scale = subId->m_scale;
                    break;
                case 0x0C: 
                    g_rxDiagnosisCache.m_o2ValveDutyCycle = (uint8_t)value;
                    g_rxDiagnosisCache.m_o2ValveDutyCycle_scale = subId->m_scale;
                    break;   
                case 0x0D: 
                    g_rxDiagnosisCache.m_exhValvePressureCtrl = (uint8_t)value;
                    g_rxDiagnosisCache.m_exhValvePressureCtrl_scale = subId->m_scale;
                    break;
                case 0x0E: 
                    g_rxDiagnosisCache.m_exhValveCurrent = (uint16_t)value;
                    g_rxDiagnosisCache.m_exhValveCurrent_scale = subId->m_scale;
                    break;  
                case 0x0F: 
                    g_rxDiagnosisCache.m_exhValveDutyCycle = (uint8_t)value;
                    g_rxDiagnosisCache.m_exhValveDutyCycle_scale = subId->m_scale;
                    break;   
                default:
                    break;
            }
            g_rxDiagnosisCache.m_valid[subId->m_subId] = true;
            DiagnosesCtrlFlagSet(subId->m_subId);
        }
    }
}

/**
 * @brief 更新系统菜单缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxDataQueryCache(const ProtocolPacket_t* packet)
{
    for (uint8_t i = 0; i < packet->m_subIdCount; i++) {
        const ProtocolSubId_t* subId = &packet->m_subIds[i];
        if (subId->m_isValid && subId->m_subId < PROTOCOL_DATA_QUERY_SUBID_MAX) {
            uint16_t value = 0;
            if (subId->m_dataSize == 1) {
                value = packet->m_payload[subId->m_dataOffset];
            } else if (subId->m_dataSize == 2) {
                value = packet->m_payload[subId->m_dataOffset] | 
                       (packet->m_payload[subId->m_dataOffset + 1] << 8);
            }
            
            /* 根据子ID更新对应的数据查询字段 */
            switch (subId->m_subId) {
                case 0x00: 
                    g_rxDataQueryCache.m_queryVersion = (uint8_t)value;
                    g_rxDataQueryCache.m_queryVersion_scale = subId->m_scale;
                    break;
                case 0x01: 
                    g_rxDataQueryCache.m_queryBootSelf = (uint8_t)value;
                    g_rxDataQueryCache.m_queryBootSelf_scale = subId->m_scale;
                    break;
                case 0x02: 
                    g_rxDataQueryCache.m_querySystemSelf = (uint8_t)value;
                    g_rxDataQueryCache.m_querySystemSelf_scale = subId->m_scale;
                    break;
                case 0x03: 
                    g_rxDataQueryCache.m_queryMonitor = (uint8_t)value;
                    g_rxDataQueryCache.m_queryMonitor_scale = subId->m_scale;
                    break;
                case 0x04: 
                    g_rxDataQueryCache.m_queryDiagnosis = (uint8_t)value;
                    g_rxDataQueryCache.m_queryDiagnosis_scale = subId->m_scale;
                    break;
                case 0x05: 
                    g_rxDataQueryCache.m_queryUserZero = (uint8_t)value;
                    g_rxDataQueryCache.m_queryUserZero_scale = subId->m_scale;
                    break;
                case 0x06: 
                    g_rxDataQueryCache.m_queryFactoryZero = (uint8_t)value;
                    g_rxDataQueryCache.m_queryFactoryZero_scale = subId->m_scale;
                    break;
                case 0x07: 
                    g_rxDataQueryCache.m_queryPressCalib = (uint8_t)value;
                    g_rxDataQueryCache.m_queryPressCalib_scale = subId->m_scale;
                    break;
                case 0x08: 
                    g_rxDataQueryCache.m_queryUserProx = (uint8_t)value;
                    g_rxDataQueryCache.m_queryUserProx_scale = subId->m_scale;
                    break;
                case 0x09: 
                    g_rxDataQueryCache.m_queryFactoryProx = (uint8_t)value;
                    g_rxDataQueryCache.m_queryFactoryProx_scale = subId->m_scale;
                    break;
                case 0x0A: 
                    g_rxDataQueryCache.m_queryUserO2 = (uint8_t)value;
                    g_rxDataQueryCache.m_queryUserO2_scale = subId->m_scale;
                    break;
                case 0x0B: 
                    g_rxDataQueryCache.m_queryFactoryO2 = (uint8_t)value;
                    g_rxDataQueryCache.m_queryFactoryO2_scale = subId->m_scale;
                    break;
                case 0x0C: 
                    g_rxDataQueryCache.m_queryO2Valve = (uint8_t)value;
                    g_rxDataQueryCache.m_queryO2Valve_scale = subId->m_scale;
                    break;
                case 0x0D: 
                    g_rxDataQueryCache.m_queryAirO2Coeff = (uint8_t)value;
                    g_rxDataQueryCache.m_queryAirO2Coeff_scale = subId->m_scale;
                    break;
                case 0x0E: 
                    g_rxDataQueryCache.m_queryExpValve = (uint8_t)value;
                    g_rxDataQueryCache.m_queryExpValve_scale = subId->m_scale;
                    break;
                case 0x0F: 
                    g_rxDataQueryCache.m_queryO2SrcCalib = (uint8_t)value;
                    g_rxDataQueryCache.m_queryO2SrcCalib_scale = subId->m_scale;
                    break;
            }
            g_rxDataQueryCache.m_valid[subId->m_subId] = true;
        }
    }
}

void ProtocolUpdateRxOnlineUpgradeCache(const ProtocolPacket_t* packet)
{
    for (uint8_t i = 0; i < packet->m_subIdCount; i++) {
        const ProtocolSubId_t* subId = &packet->m_subIds[i];

        if ((!subId->m_isValid) ||
            (subId->m_subId >= PROTOCOL_ONLINE_UPGRADE_SUBID_MAX) ||
            (subId->m_dataSize < 1)) {
            continue;
        }

        switch (subId->m_subId) {
            case 0x00:
                g_rxOnlineUpgradeCache.m_upgradeCommand = packet->m_payload[subId->m_dataOffset];
                g_rxOnlineUpgradeCache.m_upgradeCommand_scale = subId->m_scale;

                if ((g_rxOnlineUpgradeCache.m_upgradeCommand == PROTOCOL_UPGRADE_FLAG_SET_VALUE) ||
                    (g_rxOnlineUpgradeCache.m_upgradeCommand == PROTOCOL_UPGRADE_FLAG_CLEAR_VALUE)) {
                    g_rxOnlineUpgradeCache.m_valid[subId->m_subId] =
                        ProtocolWriteUpgradeFlag(g_rxOnlineUpgradeCache.m_upgradeCommand);
                } else {
                    g_rxOnlineUpgradeCache.m_valid[subId->m_subId] = false;
                }
                break;

            default:
                break;
        }
    }
}

/* ==================== 发送数据预处理函数 ==================== */
/**
 * @brief 从缓存发送监测参数
 * @param instance USART实例
 */
#endif
#if 0 /* Legacy machine features retained for later migration. */
void ProtocolSendMonitorParamsFromCache(uint8_t instance)
{
    uint8_t TxData[256];
    SubIDCache_t subIds[PROTOCOL_MONITOR_PARAMS_SUBID_MAX];
    uint8_t count = 0;
    //SEGGER_RTT_printf(0, "[DEBUG] ProtocolSendMonitorParamsFromCache called\n");
    
    for (uint8_t i = 0; i < PROTOCOL_MONITOR_PARAMS_SUBID_MAX; i++) {
        if (g_txMonitorParamsCache.m_valid[i]) {
            SubIDCache_t* subId = &subIds[count];
            subId->m_id = i;
            
            switch (i) {
                case 0x00: 
                    subId->m_value = g_txMonitorParamsCache.m_fio2;
                    subId->m_size = E_FIO2_SIZE;
                    subId->m_scale = E_FIO2_SCALE;
                    break;
                case 0x01: 
                    subId->m_value = g_txMonitorParamsCache.m_pPeak;
                    subId->m_size = E_PPEAK_SIZE;
                    subId->m_scale = E_PPEAK_SCALE;
                    break;
                case 0x02: 
                    subId->m_value = g_txMonitorParamsCache.m_pPlat;
                    subId->m_size = E_PPLAT_SIZE;
                    subId->m_scale = E_PPLAT_SCALE;
                    break;
                case 0x03: 
                    subId->m_value = g_txMonitorParamsCache.m_pMean;
                    subId->m_size = E_PMEAN_SIZE;
                    subId->m_scale = E_PMEAN_SCALE;
                    break;
                case 0x04: 
                    subId->m_value = g_txMonitorParamsCache.m_peep;
                    subId->m_size = E_PEEP_SIZE;
                    subId->m_scale = E_PEEP_SCALE;
                    break;
                case 0x05:               
                    subId->m_value = g_txMonitorParamsCache.m_tvi;
                    subId->m_size = E_TVI_SIZE;
                    subId->m_scale = E_TVI_SCALE;
                    break;
                case 0x06: 
                    subId->m_value = g_txMonitorParamsCache.m_tve;
                    subId->m_size = E_TVE_SIZE;
                    subId->m_scale = E_TVE_SCALE;
                    break;
                case 0x07: 
                    subId->m_value = g_txMonitorParamsCache.m_tveSpn;
                    subId->m_size = E_TVESPN_SIZE;
                    subId->m_scale = E_TVESPN_SCALE;
                    break;
                case 0x08: 
                    subId->m_value = g_txMonitorParamsCache.m_mvi;
                    subId->m_size = E_MVI_SIZE;
                    subId->m_scale = E_MVI_SCALE;
                    break;
                case 0x09: 
                    subId->m_value = g_txMonitorParamsCache.m_mve;
                    subId->m_size = E_MVE_SIZE;
                    subId->m_scale = E_MVE_SCALE;
                    break;
                case 0x0A: 
                    subId->m_value = g_txMonitorParamsCache.m_mvSpn;
                    subId->m_size = E_MVSPN_SIZE;
                    subId->m_scale = E_MVSPN_SCALE;
                    break;
                case 0x0B: 
                    subId->m_value = g_txMonitorParamsCache.m_mvLeak;
                    subId->m_size = E_MVLEAK_SIZE;
                    subId->m_scale = E_MVLEAK_SCALE;
                    break;
                case 0x0C: 
                    subId->m_value = g_txMonitorParamsCache.m_leakPercent;
                    subId->m_size = E_LEAKPERCENT_SIZE;
                    subId->m_scale = E_LEAKPERCENT_SCALE;
                    break;
                case 0x0D: 
                    subId->m_value = g_txMonitorParamsCache.m_flow;
                    subId->m_size = E_FLOW_SIZE;
                    subId->m_scale = E_FLOW_SCALE;
                    break;
                case 0x0E: 
                    subId->m_value = g_txMonitorParamsCache.m_inspFlow;
                    subId->m_size = E_INSPFLOW_SIZE;
                    subId->m_scale = E_INSPFLOW_SCALE;
                    break;
                case 0x0F: 
                    subId->m_value = g_txMonitorParamsCache.m_expFlow;
                    subId->m_size = E_EXPFLOW_SIZE;
                    subId->m_scale = E_EXPFLOW_SCALE;
                    break;
                case 0x10: 
                    subId->m_value = g_txMonitorParamsCache.m_frTotal;
                    subId->m_size = E_FRTOTAL_SIZE;
                    subId->m_scale = E_FRTOTAL_SCALE;
                    break;
                case 0x11: 
                    subId->m_value = g_txMonitorParamsCache.m_frMand;
                    subId->m_size = E_FRMAND_SIZE;
                    subId->m_scale = E_FRMAND_SCALE;
                    break;
                case 0x12: 
                    subId->m_value = g_txMonitorParamsCache.m_frSpn;
                    subId->m_size = E_FRSPN_SIZE;
                    subId->m_scale = E_FRSPN_SCALE;
                    break;
                case 0x13: 
                    subId->m_value = g_txMonitorParamsCache.m_ti;
                    subId->m_size = E_TI_SIZE;
                    subId->m_scale = E_TI_SCALE;
                    break;
                case 0x14: 
                    subId->m_value = g_txMonitorParamsCache.m_te;
                    subId->m_size = E_TE_SIZE;
                    subId->m_scale = E_TE_SCALE;
                    break;
                case 0x15: 
                    memcpy((uint8_t*)&(subId->m_value),(uint8_t *)&g_txMonitorParamsCache.m_ie,E_IE_SIZE);
                    subId->m_size = E_IE_SIZE;
                    subId->m_scale = E_IE_SCALE;
                    break;
                case 0x16: 
                    subId->m_value = g_txMonitorParamsCache.m_rInsp;
                    subId->m_size = E_RINSP_SIZE;
                    subId->m_scale = E_RINSP_SCALE;
                    break;
                case 0x17: 
                    subId->m_value = g_txMonitorParamsCache.m_rExp;
                    subId->m_size = E_REXP_SIZE;
                    subId->m_scale = E_REXP_SCALE;
                    break;
                case 0x18: 
                    subId->m_value = g_txMonitorParamsCache.m_cStat;
                    subId->m_size = E_CSTAT_SIZE;
                    subId->m_scale = E_CSTAT_SCALE;
                    break;
                case 0x19: 
                    subId->m_value = g_txMonitorParamsCache.m_cDyn;
                    subId->m_size = E_CDYN_SIZE;
                    subId->m_scale = E_CDYN_SCALE;
                    break;
                case 0x1A: 
                    subId->m_value = g_txMonitorParamsCache.m_rcexp;
                    subId->m_size = E_RCEXP_SIZE;
                    subId->m_scale = E_RCEXP_SCALE;
                    break;
                case 0x1B: 
                    subId->m_value = g_txMonitorParamsCache.m_wob;
                    subId->m_size = E_WOB_SIZE;
                    subId->m_scale = E_WOB_SCALE;
                    break;
                case 0x1C: 
                    subId->m_value = g_txMonitorParamsCache.m_peepi;
                    subId->m_size = E_PEEPI_SIZE;
                    subId->m_scale = E_PEEPI_SCALE;
                    break;
                case 0x1D: 
                    subId->m_value = g_txMonitorParamsCache.m_peepTotal;
                    subId->m_size = E_PEEPTOTAL_SIZE;
                    subId->m_scale = E_PEEPTOTAL_SCALE;
                    break;
                case 0x1E: 
                    subId->m_value = g_txMonitorParamsCache.m_p01;
                    subId->m_size = E_P01_SIZE;
                    subId->m_scale = E_P01_SCALE;
                    break;
                case 0x1F: 
                    subId->m_value = g_txMonitorParamsCache.m_nif;
                    subId->m_size = E_NIF_SIZE;
                    subId->m_scale = E_NIF_SCALE;
                    break;
                case 0x20: 
                    subId->m_value = g_txMonitorParamsCache.m_ptp;
                    subId->m_size = E_PTP_SIZE;
                    subId->m_scale = E_PTP_SCALE;
                    break;
                case 0x21: 
                    subId->m_value = g_txMonitorParamsCache.m_tveIbw;
                    subId->m_size = E_TVEIBW_SIZE;
                    subId->m_scale = E_TVEIBW_SCALE;
                    break;
                case 0x22: 
                    subId->m_value = g_txMonitorParamsCache.m_o2SrcPress;
                    subId->m_size = E_O2SRCPRESS_SIZE;
                    subId->m_scale = E_O2SRCPRESS_SCALE;
                    break;
                case 0x23:
                    // Reserved for future use
                    break;
                case 0x24:
                    // Reserved for future use
                    break;
                case 0x25:  // 牵张指数
                    subId->m_value = g_txMonitorParamsCache.m_stretchIndex;
                    subId->m_size = E_STRETCH_INDEX_SIZE;
                    subId->m_scale = E_STRETCH_INDEX_SCALE;
                    break;
                case 0x26:  // 肺过度膨胀系数
                    subId->m_value = g_txMonitorParamsCache.m_lungOverinflation;
                    subId->m_size = E_LUNG_OVERINFLATION_SIZE;
                    subId->m_scale = E_LUNG_OVERINFLATION_SCALE;
                    break;
                case 0x27:  // 单次呼吸二氧化碳排放量
                    subId->m_value = g_txMonitorParamsCache.m_brCo2Output;
                    subId->m_size = E_BR_CO2_OUTPUT_SIZE;
                    subId->m_scale = E_BR_CO2_OUTPUT_SCALE;
                    break;
                case 0x28:  // 血氧饱和度/吸入氧浓度
                    subId->m_value = g_txMonitorParamsCache.m_spO2FiO2Ratio;
                    subId->m_size = E_SPO2_FIO2_RATIO_SIZE;
                    subId->m_scale = E_SPO2_FIO2_RATIO_SCALE;
                    break;
                case 0x29:  // 氧饱和度指数
                    subId->m_value = g_txMonitorParamsCache.m_oxygenIndex;
                    subId->m_size = E_OXYGEN_INDEX_SIZE;
                    subId->m_scale = E_OXYGEN_INDEX_SCALE;
                    break;
                case 0x2A:  // 平均压与氧浓度乘积
                    subId->m_value = g_txMonitorParamsCache.m_meanPressO2Product;
                    subId->m_size = E_MEAN_PRESS_O2_PRODUCT_SIZE;
                    subId->m_scale = E_MEAN_PRESS_O2_PRODUCT_SCALE;
                    break;
                case 0x2B:  // ROX指数
                    subId->m_value = g_txMonitorParamsCache.m_roxIndex;
                    subId->m_size = E_ROX_INDEX_SIZE;
                    subId->m_scale = E_ROX_INDEX_SCALE;
                    break;
                case 0x2C:  // 机械能
                    subId->m_value = g_txMonitorParamsCache.m_mechEnergy;
                    subId->m_size = E_MECH_ENERGY_SIZE;
                    subId->m_scale = E_MECH_ENERGY_SCALE;
                    break;
                case 0x2D:  // 驱动压
                    subId->m_value = g_txMonitorParamsCache.m_drivingPressure;
                    subId->m_size  = E_DRIVING_PRESSURE_SIZE;
                    subId->m_scale = E_DRIVING_PRESSURE_SCALE;
                    break;
                case 0x2E:  // 
                    subId->m_value = g_txMonitorParamsCache.m_O2TherapyFlow;
                    subId->m_size  = E_O2_THERAPY_FLOW_SIZE;
                    subId->m_scale = E_O2_THERAPY_FLOW_SCALE;
                    break;
                case 0x2F: // 陷闭气体预留
                    break;
                case 0x30:  // 浅快呼吸指数
                    subId->m_value = g_txMonitorParamsCache.m_RSBI;
                    subId->m_size  = E_RSBI_SIZE;
                    subId->m_scale = E_RSBI_SCALE;
                    break;

                default:
                    // 默认配置
                    subId->m_value = 0;
                    subId->m_size = 2;
                    subId->m_scale = 0;
                    break;
            }
            
            count++;
            if (count >= 32) {
                break;
            }
        }
    }
    
//    SEGGER_RTT_printf(0, "[DEBUG] Found %d valid monitor params\n", count);
    
    if (count > 0) {
//        SEGGER_RTT_printf(0, "[DEBUG] Sending monitor params...\n");
        
        uint16_t SendLen = ProtocolCreateSubIdData(TxData,PROTOCOL_ADDR_VCM_TO_MCM,false,
                                                    PROTOCOL_TX_MID_MONITOR_PARAMS,(uint8_t *)subIds, count);
        if (SendLen > 0) {
            if(ProtocolSendData(instance, PROTOCOL_PRIORITY_HIGH, TxData, SendLen) == PROTOCOL_OK) {
//                SEGGER_RTT_printf(0, "[DEBUG] Monitor params sent successfully\n");
                // 发送成功后清除所有有效标志
                for (uint8_t i = 0; i < PROTOCOL_MONITOR_PARAMS_SUBID_MAX; i++) {
                    g_txMonitorParamsCache.m_valid[i] = false;
                }
            } else {
//                SEGGER_RTT_printf(0, "[DEBUG] Failed to send monitor params: buffer full\n");
            }       
        } else {
//            SEGGER_RTT_printf(0, "[DEBUG] No monitor params to send\n");
        }
    }
}

/**
 * @brief 从缓存发送监测参数
 * @param instance USART实例
 */
void ProtocolSendResultMsgFromCache(uint8_t instance)
{
    uint8_t TxData[256];
    SubIDCache_t subIds[PROTOCOL_RESULT_MSG_SUBID_MAX];
    uint8_t count = 0;
    
    for (uint8_t i = 0; i < PROTOCOL_RESULT_MSG_SUBID_MAX; i++) {
        if (g_txResultMsgCache.m_valid[i]) {
            SubIDCache_t* subId = &subIds[count];
            subId->m_id = i;           
            switch (i) {
                case 0x00: 
                    subId->m_value = g_txResultMsgCache.m_progress;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x01: 
                    subId->m_value = g_txResultMsgCache.m_bootSelfResult;
                    subId->m_size = 5;  
                    subId->m_scale = 0;
                    break;
                case 0x02:  
                    subId->m_value = g_txResultMsgCache.m_reserved02;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x03:  
                    subId->m_value = g_txResultMsgCache.m_turbineTest;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x04:
                    subId->m_value = g_txResultMsgCache.m_o2FlowSensorTest;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x05:
                    subId->m_value = g_txResultMsgCache.m_inspFlowSensorTest;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x06:
                    subId->m_value = g_txResultMsgCache.m_pressureSensorTest;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x07:
                    subId->m_value = g_txResultMsgCache.m_expValveTest;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x08:
                    subId->m_value = g_txResultMsgCache.m_safetyValveTest;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x09:
                    subId->m_value = g_txResultMsgCache.m_leakTest;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x0A:
                    subId->m_value = g_txResultMsgCache.m_complianceTest;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x0B:
                    subId->m_value = g_txResultMsgCache.m_pipeResistanceTest;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x0C:
                    subId->m_value = g_txResultMsgCache.m_proxFlowSensorTest;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x0D:
                    subId->m_value = g_txResultMsgCache.m_o2SensorTest;
                    subId->m_size = 1;
                    subId->m_scale = 0;
                    break;
                case 0x0E:
                    subId->m_value = g_txResultMsgCache.m_pipeLeakage;
                    subId->m_size = 2;
                    subId->m_scale = 1;
                    break;
                case 0x0F:
                    subId->m_value = g_txResultMsgCache.m_pipeCompliance;
                    subId->m_size = 2;
                    subId->m_scale = 2;
                    break;
                case 0x10:
                    subId->m_value = g_txResultMsgCache.m_pipeResistance;
                    subId->m_size = 2;
                    subId->m_scale = 2;
                    break;
                default:
                    // 默认配置
                    subId->m_value = 0;
                    subId->m_size = 2;
                    subId->m_scale = 0;
                    break;
            }
            g_txResultMsgCache.m_valid[i] = 0;
            count++;
        }
    }
    
    if (count > 0) {
        uint16_t SendLen = ProtocolCreateSubIdData(TxData,PROTOCOL_ADDR_VCM_TO_MCM,false,
                                                    PROTOCOL_TX_MID_RESULT_MSG,(uint8_t *)subIds,count);
        if (SendLen > 0) {
            if(ProtocolSendData(instance, PROTOCOL_PRIORITY_HIGH, TxData, SendLen) != PROTOCOL_OK) {
                //SEGGER_RTT_printf(0, "[DEBUG] Failed to send ResultMsg: buffer full\n");
            }       
        } else {
            //SEGGER_RTT_printf(0, "[DEBUG] No ResultMsg to send\n");
        }
    }
}

/* 其他发送预处理函数... */
bool ProtocolHasValidSpecialFunc(void) { return false; }
void ProtocolSendSpecialFuncFromCache(uint8_t instance) { }
bool ProtocolHasValidCalibData(void) { return false; }
void ProtocolSendCalibDataFromCache(uint8_t instance) { }
bool ProtocolHasValidDiagData(void) { return false; }
void ProtocolSendDiagDataFromCache(uint8_t instance) { }
bool ProtocolHasValidResultMsg(void) { return false; }
bool ProtocolHasValidErrorRequest(void) { return false; }
void ProtocolSendErrorRequestFromCache(uint8_t instance) { }

/* ==================== 发送数据处理函数 ==================== */
#endif
/** Saturate wire values and avoid undefined conversions from invalid sensor data. */
static int32_t protocolWaveValue(float value, int32_t minimum, int32_t maximum) {
    if (!isfinite(value)) { return 0; }
    if (value < (float)minimum) { return minimum; }
    if (value > (float)maximum) { return maximum; }
    return (int32_t)value;
}

/** Send the currently available completed-breath monitor values. */
static void protocolMonitorSubIdAppend(SubIDCache_t *subIds, uint8_t *count,
                                       uint8_t id, uint64_t value,
                                       uint8_t size, uint8_t scale)
{
    if (!g_txMonitorParamsCache.m_valid[id]) {
        return;
    }
    subIds[*count].m_id = id;
    subIds[*count].m_value = value;
    subIds[*count].m_size = size;
    subIds[*count].m_scale = scale;
    ++(*count);
}

void ProtocolSendMonitorParamsFromCache(uint8_t instance)
{
#if PROTOCOL_MONITOR_PARAMS_SEND_ENABLE
    static uint8_t lTxData[64];
    static SubIDCache_t lSubIds[13];
    uint64_t lIeValue = 0U;
    uint8_t lCount = 0U;

    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x01U,
        (uint16_t)g_txMonitorParamsCache.m_pPeak, E_PPEAK_SIZE, E_PPEAK_SCALE);
    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x02U,
        (uint16_t)g_txMonitorParamsCache.m_pPlat, E_PPLAT_SIZE, E_PPLAT_SCALE);
    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x04U,
        g_txMonitorParamsCache.m_peep, E_PEEP_SIZE, E_PEEP_SCALE);
    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x05U,
        g_txMonitorParamsCache.m_tvi, E_TVI_SIZE, E_TVI_SCALE);
    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x06U,
        g_txMonitorParamsCache.m_tve, E_TVE_SIZE, E_TVE_SCALE);
    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x07U,
        g_txMonitorParamsCache.m_tveSpn, E_TVESPN_SIZE, E_TVESPN_SCALE);
    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x0EU,
        g_txMonitorParamsCache.m_inspFlow, E_INSPFLOW_SIZE, E_INSPFLOW_SCALE);
    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x10U,
        g_txMonitorParamsCache.m_frTotal, E_FRTOTAL_SIZE, E_FRTOTAL_SCALE);
    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x11U,
        g_txMonitorParamsCache.m_frMand, E_FRMAND_SIZE, E_FRMAND_SCALE);
    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x12U,
        g_txMonitorParamsCache.m_frSpn, E_FRSPN_SIZE, E_FRSPN_SCALE);
    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x13U,
        g_txMonitorParamsCache.m_ti, E_TI_SIZE, E_TI_SCALE);
    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x14U,
        g_txMonitorParamsCache.m_te, E_TE_SIZE, E_TE_SCALE);
    memcpy(&lIeValue, &g_txMonitorParamsCache.m_ie,
           sizeof(g_txMonitorParamsCache.m_ie));
    protocolMonitorSubIdAppend(lSubIds, &lCount, 0x15U,
        lIeValue, E_IE_SIZE, E_IE_SCALE);

    if (lCount > 0U) {
        uint16_t lSendLength = ProtocolCreateSubIdData(
            lTxData, PROTOCOL_ADDR_VCM_TO_MCM, false,
            PROTOCOL_TX_MID_MONITOR_PARAMS, (uint8_t *)lSubIds, lCount);
        if ((lSendLength > 0U) &&
            (ProtocolSendData(instance, PROTOCOL_PRIORITY_HIGH,
                              lTxData, lSendLength) == PROTOCOL_OK)) {
            memset(g_txMonitorParamsCache.m_valid, 0,
                   sizeof(g_txMonitorParamsCache.m_valid));
        }
    }
#else
    (void)instance;
#endif
}

/** Publish each completed breath once; retry cached data if the TX queue is full. */
void ProtocolDetectDataPreProcess(uint8_t instance, uint32_t taskCounter)
{
#if PROTOCOL_MONITOR_PARAMS_SEND_ENABLE
    static uint32_t lLastSequence = UINT32_MAX;
    stBreathResult lResult;
    uint32_t lExpiratoryTimeMs;
    uint16_t lFrequency;
    float lCycleTimeMs;
    float lInspiratoryTimeMs;

    if ((taskCounter % 50U) != 0U) {
        return;
    }
    if (!breathSchedulerRunningGet()) {
        memset(g_txMonitorParamsCache.m_valid, 0,
               sizeof(g_txMonitorParamsCache.m_valid));
        lLastSequence = UINT32_MAX;
        return;
    }

    if ((monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS) &&
        ((lResult.validMask & BREATH_RESULT_VALID_COMPLETE) != 0U) &&
        (lResult.sequence != lLastSequence)) {
        lLastSequence = lResult.sequence;
        memset(g_txMonitorParamsCache.m_valid, 0,
               sizeof(g_txMonitorParamsCache.m_valid));
        lCycleTimeMs = monitorEngineGet(MONITOR_LAST_CYCLE_TIME_MS);
        lInspiratoryTimeMs = monitorEngineGet(MONITOR_LAST_INSP_TIME_MS);

        if ((lResult.validMask & BREATH_RESULT_VALID_PPEAK) != 0U) {
            g_txMonitorParamsCache.m_pPeak = (int16_t)protocolWaveValue(
                monitorEngineGet(MONITOR_LAST_PPEAK) *
                    (float)ProtocolGetScale(E_PPEAK_SCALE),
                INT16_MIN, INT16_MAX);
            g_txMonitorParamsCache.m_valid[0x01U] = true;
        }
        if ((lResult.validMask & BREATH_RESULT_VALID_PLATEAU_PRESSURE) != 0U) {
            g_txMonitorParamsCache.m_pPlat = (int16_t)protocolWaveValue(
                monitorEngineGet(MONITOR_LAST_PLATEAU_PRS) *
                    (float)ProtocolGetScale(E_PPLAT_SCALE),
                INT16_MIN, INT16_MAX);
            g_txMonitorParamsCache.m_valid[0x02U] = true;
        }
        if ((lResult.validMask & BREATH_RESULT_VALID_PEEP) != 0U) {
            g_txMonitorParamsCache.m_peep = (uint16_t)protocolWaveValue(
                monitorEngineGet(MONITOR_LAST_PEEP) *
                    (float)ProtocolGetScale(E_PEEP_SCALE),
                0, UINT16_MAX);
            g_txMonitorParamsCache.m_valid[0x04U] = true;
        }
        if ((lResult.validMask & BREATH_RESULT_VALID_VTI) != 0U) {
            g_txMonitorParamsCache.m_tvi = (uint16_t)protocolWaveValue(
                monitorEngineGet(MONITOR_LAST_TIDA_VOL_INSP) *
                    (float)ProtocolGetScale(E_TVI_SCALE),
                0, UINT16_MAX);
            g_txMonitorParamsCache.m_valid[0x05U] = true;
        }
        if ((lResult.validMask & BREATH_RESULT_VALID_VTE) != 0U) {
            g_txMonitorParamsCache.m_tve = (uint16_t)protocolWaveValue(
                monitorEngineGet(MONITOR_LAST_TIDA_VOL_EXP) *
                    (float)ProtocolGetScale(E_TVE_SCALE),
                0, UINT16_MAX);
            g_txMonitorParamsCache.m_valid[0x06U] = true;
            if (lResult.breathType == BREATH_TYPE_SPONTANEOUS_PRESSURE_SUPPORT) {
                g_txMonitorParamsCache.m_tveSpn = g_txMonitorParamsCache.m_tve;
                g_txMonitorParamsCache.m_valid[0x07U] = true;
            }
        }
        if ((lResult.validMask & BREATH_RESULT_VALID_PEAK_INSP_FLOW) != 0U) {
            g_txMonitorParamsCache.m_inspFlow = (uint16_t)protocolWaveValue(
                monitorEngineGet(MONITOR_LAST_PEAK_INSP_FLOW) *
                    (float)ProtocolGetScale(E_INSPFLOW_SCALE),
                0, UINT16_MAX);
            g_txMonitorParamsCache.m_valid[0x0EU] = true;
        }
        if (((lResult.validMask & BREATH_RESULT_VALID_CYCLE_TIME) != 0U) &&
            (lCycleTimeMs > 0.0F)) {
            lFrequency = (uint16_t)protocolWaveValue(
                60000.0F / lCycleTimeMs, 0, UINT8_MAX);
            g_txMonitorParamsCache.m_frTotal = lFrequency;
            g_txMonitorParamsCache.m_valid[0x10U] = true;
            if (lResult.breathType == BREATH_TYPE_SPONTANEOUS_PRESSURE_SUPPORT) {
                g_txMonitorParamsCache.m_frSpn = lFrequency;
                g_txMonitorParamsCache.m_valid[0x12U] = true;
            } else if ((lResult.breathType == BREATH_TYPE_MANDATORY_PRESSURE) ||
                       (lResult.breathType == BREATH_TYPE_MANDATORY_VOLUME)) {
                g_txMonitorParamsCache.m_frMand = lFrequency;
                g_txMonitorParamsCache.m_valid[0x11U] = true;
            }
        }
        if ((lResult.validMask & BREATH_RESULT_VALID_INSPIRATORY_TIME) != 0U) {
            g_txMonitorParamsCache.m_ti = (uint16_t)protocolWaveValue(
                lInspiratoryTimeMs / 10.0F, 0, UINT16_MAX);
            g_txMonitorParamsCache.m_valid[0x13U] = true;
        }
        if (((lResult.validMask & (BREATH_RESULT_VALID_CYCLE_TIME |
                                  BREATH_RESULT_VALID_INSPIRATORY_TIME)) ==
             (BREATH_RESULT_VALID_CYCLE_TIME |
              BREATH_RESULT_VALID_INSPIRATORY_TIME)) &&
            (lCycleTimeMs >= lInspiratoryTimeMs)) {
            lExpiratoryTimeMs = (uint32_t)(lCycleTimeMs - lInspiratoryTimeMs);
            g_txMonitorParamsCache.m_te = (uint16_t)protocolWaveValue(
                (float)lExpiratoryTimeMs / 10.0F, 0, UINT16_MAX);
            g_txMonitorParamsCache.m_valid[0x14U] = true;
            if (lExpiratoryTimeMs > 0U) {
                g_txMonitorParamsCache.m_ie =
                    lInspiratoryTimeMs / (float)lExpiratoryTimeMs;
                g_txMonitorParamsCache.m_valid[0x15U] = true;
            }
        }

        /* Unavailable monitor outputs remain unset: FiO2, Pmean, minute volumes,
         * leak percentage, expiratory peak flow, resistance, compliance, RCexp,
         * WOB, PEEPi/PEEPtotal, P0.1, NIF, PTP, TVE/IBW, oxygen source pressure
         * and derived oxygen/mechanics indices. */
    }

    ProtocolSendMonitorParamsFromCache(instance);
#else
    (void)instance;
    (void)taskCounter;
#endif
}

void ProtocolWaveDataProcess(uint8_t instance, uint32_t taskCounter)
{
#if PROTOCOL_WAVE_DATA_SEND_ENABLE
    uint8_t TxData[32];
    static uint8_t WaveData[16];
    uint8_t index=0;
    /* 定时采集波形数据 - 每2个周期(20ms) */
    if(!breathSchedulerRunningGet()) {
        return;
    }
    if (taskCounter % 20 == 0) {
        // 采集波形数据并添加到FIFO
        repRtosEnterCritical();
        newWaveData.m_breathPhase = (phaseControllerStateGet() == PHASE_INSP ? 1U : 2U);  // 呼吸相位
        newWaveData.m_pressure = (int16_t)protocolWaveValue(controlDataGet(PAT_REAL_PRS)*10.0f, INT16_MIN, INT16_MAX);       // 患者压力
        newWaveData.m_flow = (int16_t)protocolWaveValue(controlDataGet(MDIFF_REAL_FLOW)*10.0f + 2000.0f, INT16_MIN, INT16_MAX);           // 患者流量
        newWaveData.m_volume = (uint16_t)protocolWaveValue(monitorEngineGet(MONITOR_TIDA_VOL), 0, UINT16_MAX);              // 总潮气量
        newWaveData.m_timestamp = taskCounter;  // 使用任务计数器作为时间戳

        repRtosExitCritical();
        WaveData[index++] = newWaveData.m_breathPhase;  // 呼吸相位
        
        WaveData[index++] = newWaveData.m_pressure & 0xff;           // 患者压力
        WaveData[index++] = (newWaveData.m_pressure>>8)& 0xff;       // 患者压力
        
        WaveData[index++] = newWaveData.m_flow& 0xff;               // 患者流量
        WaveData[index++] = (newWaveData.m_flow>>8)& 0xff;          // 患者流量
        
        WaveData[index++] = newWaveData.m_volume& 0xff;                   // 总潮气量
        WaveData[index++] = (newWaveData.m_volume>>8)& 0xff;              // 总潮气量
        
        WaveData[index++] = taskCounter>>24;  // 使用任务计数器作为时间戳
        WaveData[index++] = taskCounter>>16;  // 使用任务计数器作为时间戳
        WaveData[index++] = taskCounter>>8;  // 使用任务计数器作为时间戳
        WaveData[index++] = taskCounter;  // 使用任务计数器作为时间戳

        uint16_t SendLen = ProtocolCreateDirectData(TxData,PROTOCOL_ADDR_VCM_TO_MCM,false,PROTOCOL_TX_MID_WAVE_DATA,WaveData,index);
        if (SendLen > 0) {
            ProtocolSendData(instance, PROTOCOL_PRIORITY_NORMAL, TxData, SendLen);
        }
    }
#endif
}

/** Copy heartbeat diagnostics from the single communication task. */
void protocolHeartbeatStatsGet(stProtocolHeartbeatStats *stats) {
    if (stats != NULL) { *stats = gHeartbeatStats; }
}

/** Count successful UART submissions, excluding generic echo acknowledgements. */
void protocolHeartbeatTransmitted(void) {
    ++gHeartbeatStats.transmitted;
}

/** Reply once per valid MCM heartbeat, retaining the pending reply on queue pressure. */
void ProtocolHeartbeatDataProcess(uint8_t instance, uint32_t taskCounter)
{
#if PROTOCOL_HEARTBEAT_SEND_ENABLE
    (void)taskCounter;

    if (gHeartbeatStats.pending != 0U) {
        uint8_t TxData[16];
        uint16_t SendLen = ProtocolCreateDirectData(TxData,PROTOCOL_ADDR_VCM_TO_MCM,true,PROTOCOL_TX_MID_HEARTBEAT,NULL,0);

        if ((SendLen > 0) &&
            (ProtocolSendData(instance, PROTOCOL_PRIORITY_HIGH, TxData, SendLen) == PROTOCOL_OK)) {
            --gHeartbeatStats.pending;
        }
    }
#endif // PROTOCOL_HEARTBEAT_SEND_ENABLE
}


//void ProtocolDetectDataPreProcess(uint8_t instance, uint32_t taskCounter)
//{
//#if PROTOCOL_MONITOR_PARAMS_SEND_ENABLE
//    /* 定时发送监测参数 - (100ms) */
//    if (taskCounter % 100 == 0) {
//        // 获取当前呼吸相位
//        uint8_t currentPhase = (uint8_t)iVentMonitParamsGet(VENT_MNT_CALC_RESP_PHASE_NOW);
//        static uint8_t lastPhase = RESP_PHASE_STANDBY;
//        
//        // 检查相位切换
//        if (currentPhase != lastPhase) {
//            // 呼气相切吸气相时，发送呼气相计算参数
//            if (lastPhase == RESP_PHASE_EXP && currentPhase == RESP_PHASE_INSP) {
//                // 发送呼气相参数（按缩放倍数处理）
//                g_txMonitorParamsCache.m_peep = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PEEP) * ProtocolGetScale(E_PEEP_SCALE));         // 呼末正压
//                g_txMonitorParamsCache.m_tve = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_HMI_TVE) * ProtocolGetScale(E_TVE_SCALE));        // 呼气潮气量
//                g_txMonitorParamsCache.m_tveSpn = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_SPT_TVE) * ProtocolGetScale(E_TVESPN_SCALE));  // 自主呼气潮气量
//                g_txMonitorParamsCache.m_mve = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_MECH_MV) * ProtocolGetScale(E_MVE_SCALE));        // 机械呼气分钟通气量
//                g_txMonitorParamsCache.m_mvSpn = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_SPT_MV) * ProtocolGetScale(E_MVSPN_SCALE));     // 自主呼气分钟通气量
//                g_txMonitorParamsCache.m_expFlow = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PEF) * ProtocolGetScale(E_EXPFLOW_SCALE));    // 呼气峰流量
//                g_txMonitorParamsCache.m_te = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_TEXP) * ProtocolGetScale(E_TE_SCALE));             // 呼气时间
//                g_txMonitorParamsCache.m_rExp = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_RESIST_EXP) * ProtocolGetScale(E_REXP_SCALE));   // 呼气阻力
//                g_txMonitorParamsCache.m_rcexp = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_RC_EXP) * ProtocolGetScale(E_RCEXP_SCALE));     // 呼气时间常数
//                g_txMonitorParamsCache.m_frTotal = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_TOT_FREQ) * ProtocolGetScale(E_FRTOTAL_SCALE));// 呼气时间常数  
//                g_txMonitorParamsCache.m_peepi = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PEEPI_VAL) * ProtocolGetScale(E_PEEPI_SCALE));    // 自主呼出潮气量      
//                // g_txMonitorParamsCache.m_peepTotal = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PEEP_TOTAL) * ProtocolGetScale(E_PEEPTOTAL_SCALE));// 总呼末正压
//                // g_txMonitorParamsCache.m_nif = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_NIF) * ProtocolGetScale(E_NIF_SCALE));              // 最大吸气负压
//                // g_txMonitorParamsCache.m_tveIbw = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_TVE_IBW) * ProtocolGetScale(E_TVEIBW_SCALE));    // 理想体重潮气量比
//                // 设置有效标志
//                g_txMonitorParamsCache.m_valid[0x04] = true;  // PEEP
//                g_txMonitorParamsCache.m_valid[0x06] = true;  // TVE
//                g_txMonitorParamsCache.m_valid[0x07] = true;  // TVE_SPN
//                g_txMonitorParamsCache.m_valid[0x09] = true;  // MVE
//                g_txMonitorParamsCache.m_valid[0x10] = true;  // Ftotal
//                g_txMonitorParamsCache.m_valid[0x0A] = true;  // MV_SPN
//                g_txMonitorParamsCache.m_valid[0x0F] = true;  // EXP_FLOW
//                g_txMonitorParamsCache.m_valid[0x14] = true;  // TE
//                g_txMonitorParamsCache.m_valid[0x17] = true;  // R_EXP
//                g_txMonitorParamsCache.m_valid[0x1A] = true;  // RC_EXP
//                g_txMonitorParamsCache.m_valid[0x1C] = true;  //PEEPI
//                // g_txMonitorParamsCache.m_valid[0x1D] = true;  // m_peepTotal
//                // g_txMonitorParamsCache.m_valid[0x1F] = true;  // m_nif
//                // g_txMonitorParamsCache.m_valid[0x21] = true;  // m_tveIbw

//            }
//            // 吸气相切呼气相时，发送吸气相计算参数
//            else if (lastPhase == RESP_PHASE_INSP && currentPhase == RESP_PHASE_EXP) {
//                // 发送吸气相参数（按缩放倍数处理）
//                g_txMonitorParamsCache.m_pPeak = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PPEAK) * ProtocolGetScale(E_PPEAK_SCALE));          // 峰值压
//                g_txMonitorParamsCache.m_pPlat = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PPLAT) * ProtocolGetScale(E_PPLAT_SCALE));          // 平台压
//                g_txMonitorParamsCache.m_pMean = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PMEAN) * ProtocolGetScale(E_PMEAN_SCALE));          // 平均压
//                g_txMonitorParamsCache.m_tvi = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_HMI_TVI) * ProtocolGetScale(E_TVI_SCALE));            // 吸气潮气量
//                g_txMonitorParamsCache.m_mvi = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_TOT_MVI) * ProtocolGetScale(E_MVI_SCALE));            // 吸气分钟通气量
//                g_txMonitorParamsCache.m_inspFlow = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PIF) * ProtocolGetScale(E_INSPFLOW_SCALE));      // 吸气峰流量
//                g_txMonitorParamsCache.m_ti = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_TINSP) * ProtocolGetScale(E_TI_SCALE));                // 吸气时间
//                g_txMonitorParamsCache.m_rInsp = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_RESIST_INSP) * ProtocolGetScale(E_RINSP_SCALE));    // 吸气阻力
//                g_txMonitorParamsCache.m_cDyn = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_COMPLIANCE_DYM) * ProtocolGetScale(E_CDYN_SCALE));   // 动态顺应性
//                g_txMonitorParamsCache.m_cStat = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_COMPLIANCE_STAT) * ProtocolGetScale(E_CSTAT_SCALE));// 静态顺应性
//                g_txMonitorParamsCache.m_mvSpn = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_SPT_MV) * ProtocolGetScale(E_MVSPN_SCALE));        // 自主分钟通气量
//                g_txMonitorParamsCache.m_mvLeak = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PAT_MVLEAK) * ProtocolGetScale(E_MVLEAK_SCALE));   // 自主分钟通气泄露量
//                g_txMonitorParamsCache.m_leakPercent = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PAT_LEAK_PCTG) * ProtocolGetScale(E_LEAKPERCENT_SCALE));   // 泄露百分比
//                g_txMonitorParamsCache.m_wob = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_WOB) * ProtocolGetScale(E_WOB_SCALE));              // 呼吸功

//                // 设置有效标志
//                g_txMonitorParamsCache.m_valid[0x01] = true;  // P_PEAK
//                g_txMonitorParamsCache.m_valid[0x02] = true;  // PPLAT
//                g_txMonitorParamsCache.m_valid[0x03] = true;  // PMEAN
//                g_txMonitorParamsCache.m_valid[0x04] = true;  // PEEP
//                g_txMonitorParamsCache.m_valid[0x10] = true;  // TOT_FREQ
//                g_txMonitorParamsCache.m_valid[0x11] = true;  // MECH_FREQ
//                g_txMonitorParamsCache.m_valid[0x12] = true;  // SPT_FREQ
//                g_txMonitorParamsCache.m_valid[0x1A] = true;  // RC_EXP
//            }
            
//            // 更新上次相位
//            ProtocolSendMonitorParamsFromCache(instance);
//            lastPhase = currentPhase;
//        }

//        // 始终发送基础参数（按缩放倍数处理）
//        g_txMonitorParamsCache.m_fio2 = (uint8_t)(iVentMonitParamsGet(VENT_MNT_CALC_FIO2) * ProtocolGetScale(E_FIO2_SCALE));              // 氧浓度
//        //g_txMonitorParamsCache.m_flow = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_MAX_INSP_FLOW) * ProtocolGetScale(E_FLOW_SCALE));      // 流量
//        g_txMonitorParamsCache.m_frMand = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_MECH_FREQ) * ProtocolGetScale(E_FRMAND_SCALE));      // 机控呼吸频率
//        g_txMonitorParamsCache.m_ie =  (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_IE_RATIO) * ProtocolGetScale(E_IE_SCALE));              // 吸呼比
//        g_txMonitorParamsCache.m_p01 =  (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_P01_VAL) * ProtocolGetScale(E_P01_SCALE));             // 口腔闭合压
//        g_txMonitorParamsCache.m_ptp =  (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PTP) * ProtocolGetScale(E_PTP_SCALE));                // 压力时间乘积
//        g_txMonitorParamsCache.m_frSpn = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_SPT_FREQ) * ProtocolGetScale(E_FRSPN_SCALE));
//        //g_txMonitorParamsCache.m_o2SrcPress = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_O2_SRC_PRESS) * ProtocolGetScale(E_O2SRCPRESS_SCALE));

//        g_txMonitorParamsCache.m_inspPeakFlow = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PIF) * ProtocolGetScale(E_INSP_PEAK_FLOW_SCALE));                          // 吸气峰值流速
//        g_txMonitorParamsCache.m_expPeakFlow = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PEF) * ProtocolGetScale(E_EXP_PEAK_FLOW_SCALE));           // 呼气峰值流速
//        g_txMonitorParamsCache.m_stretchIndex = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_STRESS_INDEX) * ProtocolGetScale(E_STRETCH_INDEX_SCALE));          // 牵张指数
//        g_txMonitorParamsCache.m_lungOverinflation = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_RATIO_C20_C) * ProtocolGetScale(E_LUNG_OVERINFLATION_SCALE)); // 肺过度膨胀系数
//        g_txMonitorParamsCache.m_brCo2Output = 0 ;//(uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_BR_CO2_OUTPUT) * ProtocolGetScale(E_BR_CO2_OUTPUT_SCALE));            // 单次呼吸二氧化碳排放量
//        g_txMonitorParamsCache.m_spO2FiO2Ratio = 0 ;//(uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_SPO2_FIO2_RATIO) * ProtocolGetScale(E_SPO2_FIO2_RATIO_SCALE));     // 血氧饱和度/吸入氧浓度
//        g_txMonitorParamsCache.m_oxygenIndex = 0 ;//(uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_OXYGEN_INDEX) * ProtocolGetScale(E_OXYGEN_INDEX_SCALE));             // 氧饱和度指数
//        g_txMonitorParamsCache.m_meanPressO2Product = 0 ; //(uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_MEAN_PRESS_O2_PRODUCT) * ProtocolGetScale(E_MEAN_PRESS_O2_PRODUCT_SCALE)); // 平均压与氧浓度乘积
//        g_txMonitorParamsCache.m_roxIndex = 0 ; //(uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_ROX_INDEX) * ProtocolGetScale(E_ROX_INDEX_SCALE));                       // ROX指数
//        g_txMonitorParamsCache.m_mechEnergy = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_MECH_POWER_MEAN) * ProtocolGetScale(E_MECH_ENERGY_SCALE));                 // 机械能
//        g_txMonitorParamsCache.m_drivingPressure = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_DRV_PRS) * ProtocolGetScale(E_DRIVING_PRESSURE_SCALE));  // 驱动压
//        
//        g_txMonitorParamsCache.m_valid[0x00] = true;  // FIO2
//        //g_txMonitorParamsCache.m_valid[0x0D] = true;  // FLOW_SCALE
//        g_txMonitorParamsCache.m_valid[0x11] = true;  // FRMAND
//        g_txMonitorParamsCache.m_valid[0x15] = true;  // IE
//        g_txMonitorParamsCache.m_valid[0x1E] = true;  // P01
//        g_txMonitorParamsCache.m_valid[0x20] = true;  // PTP
//        g_txMonitorParamsCache.m_valid[0x12] = true;  // m_frSpn
//        //g_txMonitorParamsCache.m_valid[0x22] = true;  // m_o2SrcPress

//        // g_txMonitorParamsCache.m_valid[0x23] = true;  // m_inspPeakFlow
//        // g_txMonitorParamsCache.m_valid[0x24] = true;  // m_expPeakFlow
//        // g_txMonitorParamsCache.m_valid[0x25] = true;  // m_stretchIndex
//        // g_txMonitorParamsCache.m_valid[0x26] = true;  // m_lungOverinflation
//        // g_txMonitorParamsCache.m_valid[0x27] = true;  // m_brCo2Output
//        // g_txMonitorParamsCache.m_valid[0x28] = true;  // m_spO2FiO2Ratio
//        // g_txMonitorParamsCache.m_valid[0x29] = true;  // m_oxygenIndex
//        // g_txMonitorParamsCache.m_valid[0x2A] = true;  // m_meanPressO2Product
//        // g_txMonitorParamsCache.m_valid[0x2B] = true;  // m_roxIndex
//        // g_txMonitorParamsCache.m_valid[0x2C] = true;  // m_mechEnergy
//        // g_txMonitorParamsCache.m_valid[0x2D] = true;  // m_drivingPressure

//        
//    }
//#endif
//}
#if 0 /* Legacy machine features retained for later migration. */
void ProtocolDetectDataPreProcess(uint8_t instance, uint32_t taskCounter)
{
    static uint8_t MntDataSendCnt = 0;
#if PROTOCOL_MONITOR_PARAMS_SEND_ENABLE
    /* 定时发送监测参数 - (100ms) */
    if (taskCounter % 50 == 0) 
    {
        
        if (MntDataSendCnt == 1)
        {
            g_txMonitorParamsCache.m_tvi        = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_HMI_TVI) * ProtocolGetScale(E_TVI_SCALE)); 
            g_txMonitorParamsCache.m_tve        = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_HMI_TVE) * ProtocolGetScale(E_TVE_SCALE)); 
            g_txMonitorParamsCache.m_tveSpn     = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_SPT_TVE) * ProtocolGetScale(E_TVESPN_SCALE));
            g_txMonitorParamsCache.m_tveIbw     = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_TVE_IBW) * ProtocolGetScale(E_TVEIBW_SCALE));
            g_txMonitorParamsCache.m_inspFlow   = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PIF) * ProtocolGetScale(E_INSPFLOW_SCALE));
            g_txMonitorParamsCache.m_expFlow    = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PEF) * ProtocolGetScale(E_EXPFLOW_SCALE));
             
            g_txMonitorParamsCache.m_valid[0x05] = true;    //!< TVI
            g_txMonitorParamsCache.m_valid[0x06] = true;    //!< TVE
            g_txMonitorParamsCache.m_valid[0x07] = true;    //!< TVE_SPN
            g_txMonitorParamsCache.m_valid[0x21] = true;    //!< TVE_IBW
            g_txMonitorParamsCache.m_valid[0x0E] = true;    //!< PIF
            g_txMonitorParamsCache.m_valid[0x0F] = true;    //!< PEF

        }
        else if(MntDataSendCnt == 2)
        {
            g_txMonitorParamsCache.m_mvi           = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_TOT_MV_MEAN) * ProtocolGetScale(E_MVI_SCALE)); 
            g_txMonitorParamsCache.m_mve           = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_MECH_MV_MEAN) * ProtocolGetScale(E_MVE_SCALE));
            g_txMonitorParamsCache.m_mvSpn         = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_SPT_MV_MEAN) * ProtocolGetScale(E_MVSPN_SCALE));
            g_txMonitorParamsCache.m_leakPercent   = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PAT_LEAK_PCTG) * ProtocolGetScale(E_LEAKPERCENT_SCALE));
            g_txMonitorParamsCache.m_mvLeak        = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PAT_MVLEAK_MEAN) * ProtocolGetScale(E_MVLEAK_SCALE));
            
            g_txMonitorParamsCache.m_valid[0x08]   = true;    //!< TOT_MV
            g_txMonitorParamsCache.m_valid[0x09]   = true;    //!< MECH_MV
            g_txMonitorParamsCache.m_valid[0x0A]   = true;    //!< SPT_MV
            g_txMonitorParamsCache.m_valid[0x0C]   = true;    //!< LEAK_PCT
            g_txMonitorParamsCache.m_valid[0x0B]   = true;    //!< LEAK_MV
        }
        else if(MntDataSendCnt == 3)
        {
			eVentMode CurrentMode = (eVentMode)VentCommParamRead(VENT_COMM_VENT_MODE) ;
			
			if (VENT_MD_NCPAP_PC == CurrentMode || VENT_MD_NCPAP == CurrentMode)
			{
				g_txMonitorParamsCache.m_pPeak     = (int16_t)(nVentParamsGet(NVENT_MNT_CALCU_PPEAK) * ProtocolGetScale(E_PPEAK_SCALE));
				g_txMonitorParamsCache.m_pMean     = (int16_t)(nVentParamsGet(NVENT_MNT_CALCU_PMEAN) * ProtocolGetScale(E_PMEAN_SCALE));
				g_txMonitorParamsCache.m_peep      = (uint16_t)(nVentParamsGet(NVENT_MNT_CALCU_PEEP) * ProtocolGetScale(E_PEEP_SCALE));
				g_txMonitorParamsCache.m_frTotal   = (uint16_t)(nVentParamsGet(NVENT_MNT_CALCU_FREQ_MEAN) * ProtocolGetScale(E_FRTOTAL_SCALE));
				g_txMonitorParamsCache.m_frSpn     = (uint16_t)(nVentParamsGet(NVENT_MNT_CALCU_SPT_FREQ_MEAN) * ProtocolGetScale(E_FRSPN_SCALE));				
			}
			else
			{
				g_txMonitorParamsCache.m_pPeak     = (int16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PPEAK) * ProtocolGetScale(E_PPEAK_SCALE));
				g_txMonitorParamsCache.m_pMean     = (int16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PMEAN) * ProtocolGetScale(E_PMEAN_SCALE));
				g_txMonitorParamsCache.m_peep      = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PEEP) * ProtocolGetScale(E_PEEP_SCALE));
				g_txMonitorParamsCache.m_frTotal   = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_TOT_FREQ_MEAN) * ProtocolGetScale(E_FRTOTAL_SCALE));
				g_txMonitorParamsCache.m_frMand    = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_MECH_FREQ_MEAN) * ProtocolGetScale(E_FRMAND_SCALE));
				g_txMonitorParamsCache.m_frSpn     = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_SPT_FREQ_MEAN) * ProtocolGetScale(E_FRSPN_SCALE));	
			}
            
			
			g_txMonitorParamsCache.m_pPlat     = (int16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PPLAT) * ProtocolGetScale(E_PPLAT_SCALE));
            
            
            g_txMonitorParamsCache.m_rcexp     = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_RC_EXP) * ProtocolGetScale(E_RCEXP_SCALE));
            
            g_txMonitorParamsCache.m_valid[0x01] = true;    //!< PPEAK
            g_txMonitorParamsCache.m_valid[0x02] = true;    //!< PPLAT
            g_txMonitorParamsCache.m_valid[0x03] = true;    //!< PMEAN
            g_txMonitorParamsCache.m_valid[0x04] = true;    //!< PEEP
            g_txMonitorParamsCache.m_valid[0x10] = true;    //!< TOT_FREQ
            g_txMonitorParamsCache.m_valid[0x11] = true;    //!< MECH_FREQ
            g_txMonitorParamsCache.m_valid[0x12] = true;    //!< SPT_FREQ
            g_txMonitorParamsCache.m_valid[0x1A] = true;    //!< RC_EXP
        }
        else if(MntDataSendCnt == 4)
        {
            g_txMonitorParamsCache.m_ie        = (float)((iVentMonitParamsGet(VENT_MNT_CALC_TINSP)/MAX(iVentMonitParamsGet(VENT_MNT_CALC_TEXP),FLOAT_EPSILON)) * ProtocolGetScale(E_IE_SCALE));
            g_txMonitorParamsCache.m_ti        = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_HMI_TINSP) * ProtocolGetScale(E_TI_SCALE));
            g_txMonitorParamsCache.m_te        = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_HMI_TEXP) * ProtocolGetScale(E_TE_SCALE));
            g_txMonitorParamsCache.m_cStat     = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_COMPLIANCE_STAT) * ProtocolGetScale(E_CSTAT_SCALE));
            g_txMonitorParamsCache.m_cDyn      = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_HMI_C_DYNAMIC) * ProtocolGetScale(E_CDYN_SCALE));
            g_txMonitorParamsCache.m_rInsp     = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_RESIST_INSP) * ProtocolGetScale(E_RINSP_SCALE));
            g_txMonitorParamsCache.m_rExp      = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_RESIST_EXP) * ProtocolGetScale(E_REXP_SCALE));
            
            g_txMonitorParamsCache.m_valid[0x15] = true;    //!< IE_RATIO
            g_txMonitorParamsCache.m_valid[0x13] = true;    //!< TINSP
            g_txMonitorParamsCache.m_valid[0x14] = true;    //!< TEXP
            g_txMonitorParamsCache.m_valid[0x18] = true;    //!< COMPLIANCE_STAT
            g_txMonitorParamsCache.m_valid[0x19] = true;    //!< COMPLIANCE_DYM
            g_txMonitorParamsCache.m_valid[0x16] = true;    //!< RESIST_INSP
            g_txMonitorParamsCache.m_valid[0x17] = true;    //!< RESIST_EXP

        }
        else if(MntDataSendCnt == 5)
        {
            if ((eVentMode)VentCommParamRead(VENT_COMM_VENT_MODE) == VENT_MD_HFO)
            {
                g_txMonitorParamsCache.m_fio2  = (uint8_t)(HiflowDataGet(HFO_CALCAT_FIO2_MEAN) * ProtocolGetScale(E_FIO2_SCALE));
            }
            else
            {
                g_txMonitorParamsCache.m_fio2  = (uint8_t)(iVentMonitParamsGet(VENT_MNT_CALC_FIO2) * ProtocolGetScale(E_FIO2_SCALE));
            }
            g_txMonitorParamsCache.m_wob   = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_WOB) * ProtocolGetScale(E_WOB_SCALE));
            g_txMonitorParamsCache.m_peepi = (uint16_t)(iVentMonitParamsGet(VENT_MNT_TOOL_PEEPI_VAL) * ProtocolGetScale(E_PEEPI_SCALE));
			g_txMonitorParamsCache.m_peepTotal = (int16_t)(iVentMonitParamsGet(VENT_MNT_TOOL_PEEPTOT_VAL) * ProtocolGetScale(E_PEEPTOTAL_SCALE));
            g_txMonitorParamsCache.m_p01   = (int16_t)(iVentMonitParamsGet(VENT_MNT_TOOL_P01_VAL) * ProtocolGetScale(E_P01_SCALE));
            g_txMonitorParamsCache.m_nif   = (int16_t)(iVentMonitParamsGet(VENT_MNT_TOOL_NIF_VAL) * ProtocolGetScale(E_NIF_SCALE));
            g_txMonitorParamsCache.m_ptp   = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_PTP) * ProtocolGetScale(E_PTP_SCALE));
            
            g_txMonitorParamsCache.m_valid[0x00] = true;    //!< FIO2
            g_txMonitorParamsCache.m_valid[0x1B] = true;    //!< WOB
            g_txMonitorParamsCache.m_valid[0x1C] = true;    //!< PEEPI_VAL
			g_txMonitorParamsCache.m_valid[0x1D] = true;    //!< PEEPtotal_VAL
            g_txMonitorParamsCache.m_valid[0x1E] = true;    //!< P01_VAL
            g_txMonitorParamsCache.m_valid[0x1F] = true;    //!< NIF_VAL
            g_txMonitorParamsCache.m_valid[0x20] = true;    //!< PTP
 
        }
        else if(MntDataSendCnt == 6)
        {
            g_txMonitorParamsCache.m_stretchIndex       = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_STRESS_INDEX) * ProtocolGetScale(E_STRETCH_INDEX_SCALE));
            g_txMonitorParamsCache.m_lungOverinflation  = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_RATIO_C20_C) * ProtocolGetScale(E_LUNG_OVERINFLATION_SCALE));
            g_txMonitorParamsCache.m_mechEnergy         = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_MECH_POWER_MEAN) * ProtocolGetScale(E_MECH_ENERGY_SCALE));
            g_txMonitorParamsCache.m_drivingPressure    = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_DRV_PRS) * ProtocolGetScale(E_DRIVING_PRESSURE_SCALE));
            g_txMonitorParamsCache.m_O2TherapyFlow      = (uint16_t)(HiflowDataGet(HFO_CALCAT_FLOW_MEAN) * ProtocolGetScale(E_O2_THERAPY_FLOW_SCALE));
            g_txMonitorParamsCache.m_RSBI               = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_RSBI) * ProtocolGetScale(E_RSBI_SCALE));
			g_txMonitorParamsCache.m_brCo2Output        = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_SIGNAL_CO2_FLOW_VLA) * ProtocolGetScale(E_BR_CO2_OUTPUT_SCALE));
			g_txMonitorParamsCache.m_spO2FiO2Ratio      = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_SPO2_DIV_FIO2) * ProtocolGetScale(E_SPO2_FIO2_RATIO_SCALE));
			g_txMonitorParamsCache.m_oxygenIndex        = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_SPO2_INDEX_VAL) * ProtocolGetScale(E_OXYGEN_INDEX_SCALE));
			g_txMonitorParamsCache.m_meanPressO2Product = (uint16_t)(iVentMonitParamsGet(VENT_MNT_CALC_MAPXFIO2_VAL) * ProtocolGetScale(E_MEAN_PRESS_O2_PRODUCT_SCALE));
			
            if ((eVentMode)VentCommParamRead(VENT_COMM_VENT_MODE) == VENT_MD_HFO)
            {
                float RoxVal = HiflowDataGet(HFO_CALCAT_ROX_INDEX);
                g_txMonitorParamsCache.m_roxIndex       = (RoxVal >= (float)HFO_INVALID_CALC_RESULT) ? 0u :
                    (uint16_t)(RoxVal * ProtocolGetScale(E_ROX_INDEX_SCALE));
            }
            else
            {
                g_txMonitorParamsCache.m_roxIndex       = 0u;
            }

            g_txMonitorParamsCache.m_valid[0x25]        = true;    //!< STRESS_INDEX
            g_txMonitorParamsCache.m_valid[0x26]        = true;    //!< C20_C
            g_txMonitorParamsCache.m_valid[0x2C]        = true;    //!< MECH_POWER_MEAN
            g_txMonitorParamsCache.m_valid[0x2D]        = true;    //!< DRV_PRS
            g_txMonitorParamsCache.m_valid[0x2E]        = true;    //!< O2_THERAPY_FLOW
            g_txMonitorParamsCache.m_valid[0x30]        = true;    //!< RSBI
            g_txMonitorParamsCache.m_valid[0x2B]        = true;    //!< ROX_INDEX

            g_txMonitorParamsCache.m_valid[0x27]        = true;    //!< DRV_PRS
            g_txMonitorParamsCache.m_valid[0x28]        = true;    //!< O2_THERAPY_FLOW
            g_txMonitorParamsCache.m_valid[0x29]        = true;    //!< RSBI
            g_txMonitorParamsCache.m_valid[0x2A]        = true;    //!< ROX_INDEX
        }
        else if(MntDataSendCnt == 7)
        {

        }
        else if(MntDataSendCnt == 8)
        {
  
        }
        else if(MntDataSendCnt == 9)
        {

        }
        else if(MntDataSendCnt == 10)
        {

        }
        
        MntDataSendCnt++;
        
        if (MntDataSendCnt > 10)
        {
            MntDataSendCnt = 1;
        }
        
        // 更新上次相位
        ProtocolSendMonitorParamsFromCache(instance);
    }
#endif
}

float ProtocolPhysAlarmDataGet(uint8_t event)
{
    switch(event) {
        case AIRWAY_PRESSURE_HIGH:
            return iVentMonitParamsGet(VENT_MNT_ALARM_PEAK_HIGH);
        case AIRWAY_PRESSURE_LOW:
            return iVentMonitParamsGet(VENT_MNT_ALARM_PEAK_LOW);
        case FIO2_HIGH:
			if (VENT_MD_HFO == (eVentMode)VentCommParamRead(VENT_COMM_VENT_MODE))
			{
				return HiflowDataGet(HFO_ALARM_FIO2_HIGH) ;
			}
			else
			{
				return iVentMonitParamsGet(VENT_MNT_ALARM_FIO2_HIGH);
			}
        case FIO2_LOW:
			if (VENT_MD_HFO == (eVentMode)VentCommParamRead(VENT_COMM_VENT_MODE))
			{
				return HiflowDataGet(HFO_ALARM_FIO2_LOW) ;
			}
			else
			{
				return iVentMonitParamsGet(VENT_MNT_ALARM_FIO2_LOW);
			}
        case EXPIRATORY_TIDAL_VOLUME_HIGH:
            return iVentMonitParamsGet(VENT_MNT_ALARM_VTE_HIGH);
        case EXPIRATORY_TIDAL_VOLUME_LOW:
            return iVentMonitParamsGet(VENT_MNT_ALARM_VTE_LOW);
        case EXPIRATORY_MINUTE_VENTILATION_HIGH:
            return iVentMonitParamsGet(VENT_MNT_ALARM_MV_HIGH);
        case EXPIRATORY_MINUTE_VENTILATION_LOW:
            return iVentMonitParamsGet(VENT_MNT_ALARM_MV_LOW);
        case RESPIRATORY_RATE_HIGH:
            return iVentMonitParamsGet(VENT_MNT_ALARM_FREQ_HIGH);
        case RESPIRATORY_RATE_LOW:
            return iVentMonitParamsGet(VENT_MNT_ALARM_FREQ_LOW);
        case APNEA_ALARM:
            return iVentMonitParamsGet(VENT_MNT_ALARM_APNEA_STATE);
        case APNEA_VENTILATION_ALARM:
            return iVentMonitParamsGet(VENT_MNT_ALARM_APNEA_VENT);
        case APNEA_VENTILATION_END:
            //return iVentMonitParamsGet(VENT_MNT_ALARM_APNEA_VENT_END);
            return 0.0f;
        case PHYSALARM_RESERVE1:
            return 0.0f;
        case PHYSALARM_RESERVE2:
            return 0.0f;
        case INVERSE_VENTILATION_ALARM:
            return iVentMonitParamsGet(VENT_MNT_ALARM_IR_VENT);
        default:
            break;
    }
    return 0.0f;
}

void ProtocolPhysAlarmDataProcess(uint8_t instance, uint32_t taskCounter)
{
    uint8_t TxData[256];
    uint32_t alarmStatus;
    static uint32_t lastAlarmStatus = 0;
    if(taskCounter%2000 == 0) {
        alarmStatus = 0;
        /* 定时发送生理报警参数 */
        for(uint8_t i = 0; i < VENTILATOR_EVENT_MAX; i++) {
            if (ProtocolPhysAlarmDataGet(i) && i < 32) {
                alarmStatus |= (1 << i);
            }
        }
        
        if(alarmStatus != lastAlarmStatus) {
            lastAlarmStatus = alarmStatus;
            // 发送数据包
            uint16_t SendLen = ProtocolCreateDirectData(TxData,PROTOCOL_ADDR_VCM_TO_MCM,false,
                                                    PROTOCOL_TX_MID_PHYS_ALARM,(uint8_t*)&alarmStatus,sizeof(alarmStatus));
            if (SendLen > 0) {
                ProtocolSendData(instance, PROTOCOL_PRIORITY_HIGH, TxData, SendLen);
            }
        }
    }
}

void ProtocolTechAlarmDataProcess(uint8_t instance, uint32_t taskCounter)
{
#if PROTOCOL_TECHALARM_SEND_ENABLE == 1
    static uint32_t lastModuleValues[TECH_ALARM_MAX_MODULES] = {0};
    static PhysiologicalFaultReg physAlarm;
    static PowerFaultReg powerAlarm;
    static TechFaultReg techAlarm;
    bool forceFullReport;
    uint8_t TxData[256];
    uint8_t faultStatus[8];
    SubIDCache_t subIds[TECH_ALARM_MAX_MODULES];
    uint8_t count = 0;   
    
    /* 检测周期设置为100ms（或依任务周期），只有当报警触发或消失才发生上报 */
    if(taskCounter % 100 == 0) {
        forceFullReport = (taskCounter % 5000 == 0);
        physAlarm.value = 0;
        powerAlarm.value = 0;
        techAlarm.value = 0;
        for (uint8_t i = 0; i < TECH_ALARM_MAX_MODULES; i++) {
            uint32_t currentValue = 0;
            uint8_t faultSize = TechAlarmGetModuleFaultSize(i);
            
            if (faultSize > 0) {
                TechAlarmGetModuleFaultStatus(i, faultStatus, faultSize);
                for (uint8_t j = 0; j < faultSize; j++) {
                    currentValue |= ((uint32_t)faultStatus[j] << (8 * j));
                }
                
                /* 状态变化立即上报，同时每5s全量上报一次当前状态 */
                if (forceFullReport || (currentValue != lastModuleValues[i])) {
                    lastModuleValues[i] = currentValue;
                    
                    SubIDCache_t* subId = &subIds[count];
                    subId->m_size = faultSize;
                    subId->m_value = currentValue;
                    subId->m_id = i;
                    subId->m_scale = 0; // 无缩放
                    subId->m_valid = 1; 
                    count++;  
                }
            }
            switch(i){
                case TECH_ALARM_PHYS_MODULE_ID:
                        physAlarm.value = (uint32_t)currentValue;
                        break;
                case TECH_ALARM_TECH_MODULE_ID:
                        techAlarm.value = (uint32_t)currentValue;
                        break;
                case TECH_ALARM_POWER_MODULE_ID:
                        powerAlarm.value = (uint32_t)currentValue;
                        break;
                default:
                        break;
            }
        }

        // 发送数据包
        if (count > 0) {
            uint16_t SendLen = ProtocolCreateSubIdData(TxData,PROTOCOL_ADDR_VCM_TO_MCM,false,
                                                    PROTOCOL_TX_MID_TECH_ALARM,(uint8_t *)subIds, count);
            if (SendLen > 0) {
                ProtocolSendData(instance, PROTOCOL_PRIORITY_HIGH, TxData, SendLen);
            }
        }
    }
#endif
}

void ProtocolSelfTestDataProcess(uint8_t instance, uint32_t taskCounter)
{
#ifdef PROTOCOL_SELFTEST_SEND_ENABLE
    static bool selfTestPowerOnSent = false;
    bool powerUpStatus;
    bool mechVentStatus;
    SELF_TEST_FLAGS_t selfTestFlags;
    SelfTestResult_t selfTestResult;
    if(taskCounter%200 == 0) { // 200ms处理一次
        if(isMCMOnline == false) {
            return;
        }
        if(selfTestPowerOnSent == false) {
            SelfTestMgrCtrl(SELF_TEST_GET_POWER_UP,&powerUpStatus);
            if(powerUpStatus == true) {                          
                SelfTestMgrCtrl(SELF_TEST_READ_POWERUP_RESULT,&selfTestFlags);
                g_txResultMsgCache.m_bootSelfResult = (uint64_t)selfTestFlags.value;
                g_txResultMsgCache.m_valid[0x01] = 1;
                selfTestPowerOnSent = true;
            }
        } else { //if(g_rxSelfTestCache.m_system == 1)
            if(SystemTest.isInProgress == true){
                SelfTestMgrCtrl(SELF_TEST_GET_MECH_VENT,&mechVentStatus);
                if(mechVentStatus == true) {   
                    SystemTest.isInProgress = false;
                    SelfTestMgrCtrl(SELF_TEST_READ_MECH_VENT_RESULT,&selfTestResult);
                    switch(SystemTest.curTest+0x03)
                    {
                        case 0x03:
                            g_txResultMsgCache.m_valid[0x03] = 1;
                            g_txResultMsgCache.m_turbineTest = selfTestResult;
                            break;
                        case 0x04:
                            g_txResultMsgCache.m_valid[0x04] = 1;
                            g_txResultMsgCache.m_o2FlowSensorTest = selfTestResult;
                            break;
                        case 0x05:
                            g_txResultMsgCache.m_valid[0x05] = 1;
                            g_txResultMsgCache.m_inspFlowSensorTest = selfTestResult;
                            break;
                        case 0x06:
                            g_txResultMsgCache.m_valid[0x06] = 1;
                            g_txResultMsgCache.m_pressureSensorTest = selfTestResult;
                            break;
                        case 0x07:
                            g_txResultMsgCache.m_valid[0x07] = 1;
                            g_txResultMsgCache.m_expValveTest = selfTestResult;
                            break;
                        case 0x08:
                            g_txResultMsgCache.m_valid[0x08] = 1;
                            g_txResultMsgCache.m_safetyValveTest = selfTestResult;
                            break;
                        case 0x09:
                            g_txResultMsgCache.m_valid[0x09] = 1;
                            g_txResultMsgCache.m_leakTest = selfTestResult;
                            g_txResultMsgCache.m_valid[0x0E] = 1;
                            g_txResultMsgCache.m_pipeLeakage = ProtocolConvertSelfTestFloatToU16(MechTestLeakageGetLeakValueLMin(), 10.0f);
                            break;
                        case 0x0A:
                            g_txResultMsgCache.m_valid[0x0A] = 1;
                            g_txResultMsgCache.m_complianceTest = selfTestResult;
                            g_txResultMsgCache.m_valid[0x0F] = 1;
                            g_txResultMsgCache.m_pipeCompliance = ProtocolConvertSelfTestFloatToU16(MechTestComplianceGetValue(), 100.0f);
                            break;
                        case 0x0B:
                            g_txResultMsgCache.m_valid[0x0B] = 1;
                            g_txResultMsgCache.m_pipeResistanceTest = selfTestResult;
                            g_txResultMsgCache.m_valid[0x10] = 1;
                            g_txResultMsgCache.m_pipeResistance = ProtocolConvertSelfTestFloatToU16(MechTestTubeResistanceGetValue(), 100.0f);
                            break;
                        case 0x0C:
                            g_txResultMsgCache.m_valid[0x0C] = 1;
                            g_txResultMsgCache.m_proxFlowSensorTest = selfTestResult;
                            break;
                        case 0x0D:
                            g_txResultMsgCache.m_valid[0x0D] = 1;
                            g_txResultMsgCache.m_o2SensorTest = selfTestResult;
                            break;
                        default:
                            break;
                    }
                    SystemTest.curTest = 0xff;
                }        
        }
        
    }
//    if(taskCounter% 10000 == 0) {
//        selfTestPowerOnSent = false;
//    }
    ProtocolSendResultMsgFromCache(instance);
    }
#endif
}

void ProtocolDiagnosesSendProcess(uint8_t instance)
{
    // 发送数据
    uint8_t TxData[256]; 
    SubIDCache_t subIds[PROTOCOL_DIAG_DATA_SUBID_MAX];
    uint8_t count = 0;

    for (uint8_t i = 0; i < PROTOCOL_DIAG_DATA_SUBID_MAX; i++) {
        if (g_txDiagDataCache.m_valid[i]) {
            subIds[count].m_id = i;
            subIds[count].m_scale = 0; 
            subIds[count].m_valid = 1;
            
            switch(i) {
                case 0: subIds[count].m_value = g_txDiagDataCache.pInspAd; subIds[count].m_size = 2; break;
                case 1: subIds[count].m_value = g_txDiagDataCache.pInsp; subIds[count].m_size = 2;subIds[count].m_scale = 2;  break;
                case 2: subIds[count].m_value = g_txDiagDataCache.pPeepAd; subIds[count].m_size = 2; break;
                case 3: subIds[count].m_value = g_txDiagDataCache.pPeep; subIds[count].m_size = 2;subIds[count].m_scale = 2; break;
                case 4: subIds[count].m_value = g_txDiagDataCache.pExpAd; subIds[count].m_size = 2; break;
                case 5: subIds[count].m_value = g_txDiagDataCache.pExp; subIds[count].m_size = 2;subIds[count].m_scale = 2; break;
                case 6: subIds[count].m_value = g_txDiagDataCache.qInsp; subIds[count].m_size = 2;subIds[count].m_scale = 2; break;
                case 7: subIds[count].m_value = g_txDiagDataCache.qProxAd; subIds[count].m_size = 2; break;
                case 8: subIds[count].m_value = g_txDiagDataCache.qProx; subIds[count].m_size = 2;subIds[count].m_scale = 2; break;
                case 9: subIds[count].m_value = g_txDiagDataCache.qO2; subIds[count].m_size = 2;subIds[count].m_scale = 2; break;
                case 10: subIds[count].m_value = g_txDiagDataCache.tempInsp; subIds[count].m_size = 1; break;
                case 11: subIds[count].m_value = g_txDiagDataCache.tempO2; subIds[count].m_size = 1; break;
                case 12: subIds[count].m_value = g_txDiagDataCache.o2Conc; subIds[count].m_size = 1; break;
                case 13: subIds[count].m_value = g_txDiagDataCache.tempO2Sensor; subIds[count].m_size = 1; break;
                case 14: subIds[count].m_value = g_txDiagDataCache.humidO2Sensor; subIds[count].m_size = 1; break;
                case 15: subIds[count].m_value = g_txDiagDataCache.pressO2Sensor; subIds[count].m_size = 2; break;
                case 16: subIds[count].m_value = g_txDiagDataCache.flowO2Sensor; subIds[count].m_size = 2; subIds[count].m_scale = 2;break;
                case 17: subIds[count].m_value = g_txDiagDataCache.pAtmAd; subIds[count].m_size = 2; break;
                case 18: subIds[count].m_value = g_txDiagDataCache.pAtm; subIds[count].m_size = 2; break;
                case 19: subIds[count].m_value = g_txDiagDataCache.tempAmbientAd; subIds[count].m_size = 2; break;
                case 20: subIds[count].m_value = g_txDiagDataCache.tempAmbient; subIds[count].m_size = 1;subIds[count].m_scale = 1; break;
                case 21: subIds[count].m_value = g_txDiagDataCache.pressNeg; subIds[count].m_size = 2; break;
                case 22: subIds[count].m_value = g_txDiagDataCache.tempNeg; subIds[count].m_size = 1; break;
                case 23: subIds[count].m_value = g_txDiagDataCache.tempTurbine; subIds[count].m_size = 1; break;
                case 24: subIds[count].m_value = g_txDiagDataCache.hwVerVcmAd; subIds[count].m_size = 2; break;
                case 25: subIds[count].m_value = g_txDiagDataCache.hwVerVcm; subIds[count].m_size = 4; break;
                case 26: subIds[count].m_value = g_txDiagDataCache.turbineSpeed; subIds[count].m_size = 2; subIds[count].m_scale = 0; break;
                //case 26: subIds[count].m_value = g_txDiagDataCache.oxygenControlValveCurrentAd; subIds[count].m_size = 2; break;
                case 27: subIds[count].m_value = g_txDiagDataCache.oxygenControlValveCurrent; subIds[count].m_size = 2; break;
                case 28: subIds[count].m_value = g_txDiagDataCache.exhalationControlValveCurrentAd; subIds[count].m_size = 2; break;
                case 29: subIds[count].m_value = g_txDiagDataCache.exhalationControlValveCurrent; subIds[count].m_size = 2; break;
                case 30: subIds[count].m_value = g_txDiagDataCache.safetyValveCurrentAd; subIds[count].m_size = 2; break;
                case 31: subIds[count].m_value = g_txDiagDataCache.safetyValveCurrent; subIds[count].m_size = 2; break;
                case 32: subIds[count].m_value = g_txDiagDataCache.inhalationPressureZeroValveStatus; subIds[count].m_size = 1; break;
                case 33: subIds[count].m_value = g_txDiagDataCache.proximalPressureZeroValveStatus; subIds[count].m_size = 1; break;
                case 34: subIds[count].m_value = g_txDiagDataCache.proximalFlowZeroValveStatus; subIds[count].m_size = 1; break;
                case 35: subIds[count].m_value = g_txDiagDataCache.flushValveStatus; subIds[count].m_size = 1; break;
                case 36: subIds[count].m_value = g_txDiagDataCache.avdd5vAD; subIds[count].m_size = 2; break;
                case 37: subIds[count].m_value = g_txDiagDataCache.avdd5v; subIds[count].m_size = 2; subIds[count].m_scale = 2;break;
                case 38: subIds[count].m_value = g_txDiagDataCache.pcmVdd3v3AD; subIds[count].m_size = 2; break;
                case 39: subIds[count].m_value = g_txDiagDataCache.pcmVdd3v3; subIds[count].m_size = 2; subIds[count].m_scale = 2;break;
                case 40: subIds[count].m_value = g_txDiagDataCache.vdd26vAD; subIds[count].m_size = 2; break;
                case 41: subIds[count].m_value = g_txDiagDataCache.vdd26v; subIds[count].m_size = 2; subIds[count].m_scale = 2;break;
                case 42: subIds[count].m_value = g_txDiagDataCache.turbo24vVoltageAD; subIds[count].m_size = 2; break;
                case 43: subIds[count].m_value = g_txDiagDataCache.turbo24vVoltage; subIds[count].m_size = 2; subIds[count].m_scale = 2;break;
                
            }
            g_txDiagDataCache.m_valid[i] = false;
            count++;
            if(count >= 24) 
                break; // 防止发送过多数据
        }
    }

    uint16_t SendLen = ProtocolCreateSubIdData(TxData,PROTOCOL_ADDR_VCM_TO_MCM,false,
                                                PROTOCOL_TX_MID_DIAG_DATA,(uint8_t *)subIds,count);
    if (SendLen > 0) {
        ProtocolSendData(instance, PROTOCOL_PRIORITY_HIGH, TxData, SendLen);
    }
}

static int16_t ProtocolDiagnosesScaleToI16(float value, float scale)
{
    return (int16_t)(value * scale + 0.5f);
}

static uint16_t ProtocolDiagnosesScaleToU16(float value, float scale)
{
    return (uint16_t)(value * scale + 0.5f);
}

static uint8_t ProtocolDiagnosesScaleToU8(float value, float scale)
{
    return (uint8_t)(value * scale + 0.5f);
}

static uint8_t ProtocolDiagnosesScaleToI8(float value, float scale)
{
    return (int8_t)(value * scale + 0.5f);
}

void ProtocolDiagnosesDataProcess(uint8_t instance, uint32_t taskCounter)
{
#if PROTOCOL_DIAGNOSES_SEND_ENABLE == 1
    if(taskCounter%250 == 0 && g_rxDiagnosisCache.m_diagnosisSwitch == true) {
        // 处理诊断数据
        const DiagnosesMgrParams_t* diagParams = DiagnosesMgrGetParams();
        g_txDiagDataCache.pInspAd = diagParams->pSensorParams->pInspAd;
        g_txDiagDataCache.pInsp = ProtocolDiagnosesScaleToI16(diagParams->pSensorParams->pInsp, 100.0f);
        g_txDiagDataCache.pPeepAd = diagParams->pSensorParams->pPeepAd;
        g_txDiagDataCache.pPeep = ProtocolDiagnosesScaleToI16(diagParams->pSensorParams->pPeep, 100.0f);
        g_txDiagDataCache.pExpAd = diagParams->pSensorParams->pExpAd;
        g_txDiagDataCache.pExp = ProtocolDiagnosesScaleToI16(diagParams->pSensorParams->pExp, 100.0f);
        g_txDiagDataCache.qInsp = ProtocolDiagnosesScaleToI16(diagParams->pSensorParams->qInsp, 100.0f);
        g_txDiagDataCache.qProxAd = diagParams->pSensorParams->qProxAd;
        g_txDiagDataCache.qProx = ProtocolDiagnosesScaleToI16(diagParams->pSensorParams->qProx, 100.0f);
        g_txDiagDataCache.qO2 = ProtocolDiagnosesScaleToI16(diagParams->pSensorParams->qO2, 100.0f);
        g_txDiagDataCache.tempInsp = diagParams->pSensorParams->tempInsp;
        g_txDiagDataCache.tempO2 = diagParams->pSensorParams->tempO2;
        g_txDiagDataCache.o2Conc = diagParams->pSensorParams->o2Conc;
        g_txDiagDataCache.tempO2Sensor = diagParams->pSensorParams->tempO2Sensor;
        g_txDiagDataCache.humidO2Sensor = diagParams->pSensorParams->humidO2Sensor;
        g_txDiagDataCache.pressO2Sensor = diagParams->pSensorParams->pressO2Sensor;
        g_txDiagDataCache.flowO2Sensor = ProtocolDiagnosesScaleToU16(diagParams->pSensorParams->flowO2Sensor, 100.0f);
        g_txDiagDataCache.pAtmAd = diagParams->pSensorParams->pAtmAd;
        g_txDiagDataCache.pAtm = diagParams->pSensorParams->pAtm;
        g_txDiagDataCache.tempAmbientAd = diagParams->pSensorParams->tempAmbientAd;
        g_txDiagDataCache.tempAmbient = ProtocolDiagnosesScaleToI8(diagParams->pSensorParams->tempAmbient, 10.0f);
        g_txDiagDataCache.pressNeg = diagParams->pSensorParams->pressNeg;
        g_txDiagDataCache.tempNeg = diagParams->pSensorParams->tempNeg;
        g_txDiagDataCache.tempTurbine = diagParams->pSensorParams->tempTurbine;
        g_txDiagDataCache.hwVerVcmAd = diagParams->pSensorParams->hwVerVcmAd;
        g_txDiagDataCache.hwVerVcm = diagParams->pSensorParams->hwVerVcm;

        g_txDiagDataCache.oxygenControlValveCurrentAd = diagParams->pActuatorParams->oxygenControlValveCurrentAd;
        g_txDiagDataCache.oxygenControlValveCurrent = diagParams->pActuatorParams->oxygenControlValveCurrent;
        g_txDiagDataCache.exhalationControlValveCurrentAd = diagParams->pActuatorParams->exhalationControlValveCurrentAd;
        g_txDiagDataCache.exhalationControlValveCurrent = diagParams->pActuatorParams->exhalationControlValveCurrent;
        g_txDiagDataCache.safetyValveCurrentAd = diagParams->pActuatorParams->safetyValveCurrentAd;
        g_txDiagDataCache.safetyValveCurrent = diagParams->pActuatorParams->safetyValveCurrent;
        g_txDiagDataCache.inhalationPressureZeroValveStatus = diagParams->pActuatorParams->inhalationPressureZeroValveStatus;
        g_txDiagDataCache.proximalPressureZeroValveStatus = diagParams->pActuatorParams->proximalPressureZeroValveStatus;
        g_txDiagDataCache.proximalFlowZeroValveStatus = diagParams->pActuatorParams->proximalFlowZeroValveStatus;
        g_txDiagDataCache.flushValveStatus = diagParams->pActuatorParams->flushValveStatus;

        g_txDiagDataCache.avdd5vAD = diagParams->pPowerParams->avdd5vAD;
        g_txDiagDataCache.avdd5v = ProtocolDiagnosesScaleToU16(diagParams->pPowerParams->avdd5v, 100.0f);
        g_txDiagDataCache.pcmVdd3v3AD = diagParams->pPowerParams->pcmVdd3v3AD;
        g_txDiagDataCache.pcmVdd3v3 = ProtocolDiagnosesScaleToU16(diagParams->pPowerParams->pcmVdd3v3, 100.0f);
        g_txDiagDataCache.vdd26vAD = diagParams->pPowerParams->vdd26vAD;
        g_txDiagDataCache.vdd26v = ProtocolDiagnosesScaleToU16(diagParams->pPowerParams->vdd26v, 100.0f);
        g_txDiagDataCache.turbo24vVoltageAD = diagParams->pPowerParams->turbo24vVoltageAD;
        g_txDiagDataCache.turbo24vVoltage = diagParams->pPowerParams->turbo24vVoltage;
        g_txDiagDataCache.turbineSpeed = ProtocolDiagnosesScaleToU16(diagParams->pSensorParams->turbineSpeed, 1.0f);

        // 设置所有字段为有效
        for(int i=0; i < PROTOCOL_DIAG_DATA_SUBID_MAX; i++) {
            g_txDiagDataCache.m_valid[i] = true;
        }
    }

    for(int i=0; i < PROTOCOL_DIAG_DATA_SUBID_MAX; i++) {
        if(g_txDiagDataCache.m_valid[i] == true) {
            ProtocolDiagnosesSendProcess(instance);
            break;
        }
    }
#endif
}

void ProtocolCalibDataSend(uint8_t instance)
{
    // 发送数据
    uint8_t TxData[256]; 
    SubIDCache_t subIds[PROTOCOL_CALIB_DATA_SUBID_MAX];
    uint8_t count = 0;

    for (uint8_t i = 0; i < PROTOCOL_CALIB_DATA_SUBID_MAX; i++) {
        if (g_txCalibDataCache.m_valid[i]) {
            subIds[count].m_id = i;
            subIds[count].m_valid = 1;
            
            switch(i) {
                case 0: subIds[count].m_value = g_txCalibDataCache.m_sendProgress; subIds[count].m_size = 1;subIds[count].m_scale = 0; break;
                case 1: subIds[count].m_value = g_txCalibDataCache.m_currentCalibItem; subIds[count].m_size = 1;subIds[count].m_scale = 0; break;
                case 2: subIds[count].m_value = g_txCalibDataCache.m_calibrationResult; subIds[count].m_size = 1;subIds[count].m_scale = 0; break;
                case 3: subIds[count].m_value = g_txCalibDataCache.m_faultCode; subIds[count].m_size = 1;subIds[count].m_scale = 0; break;
                case 4: subIds[count].m_value = g_txCalibDataCache.m_inspPressureAdZeroHist; subIds[count].m_size = 2;subIds[count].m_scale = 0; break;
                case 5: subIds[count].m_value = g_txCalibDataCache.m_expPressureAdZeroHist; subIds[count].m_size = 2;subIds[count].m_scale = 0; break;
                case 6: subIds[count].m_value = g_txCalibDataCache.m_peepPressureAdZeroHist; subIds[count].m_size = 2;subIds[count].m_scale = 0; break;
                case 7: subIds[count].m_value = g_txCalibDataCache.m_proxFlowAdZeroHist; subIds[count].m_size = 2;subIds[count].m_scale = 0; break;
                case 8: subIds[count].m_value = g_txCalibDataCache.m_inspPressureAd; subIds[count].m_size = 2;subIds[count].m_scale = 0; break;
                case 9: subIds[count].m_value = g_txCalibDataCache.m_expPressureAd; subIds[count].m_size = 2;subIds[count].m_scale = 0; break;
                case 10: subIds[count].m_value = g_txCalibDataCache.m_peepPressureAd; subIds[count].m_size = 2;subIds[count].m_scale = 0; break;
                case 11: subIds[count].m_value = g_txCalibDataCache.m_proxFlowAd; subIds[count].m_size = 2;subIds[count].m_scale = 0; break;
                case 12: subIds[count].m_value = g_txCalibDataCache.m_TestIndex; subIds[count].m_size = 1;subIds[count].m_scale = 0; break;
                case 13: subIds[count].m_value = g_txCalibDataCache.m_MotorSpeed; subIds[count].m_size = 2;subIds[count].m_scale = 0; break;
                case 14: subIds[count].m_value = g_txCalibDataCache.m_PressureValue; subIds[count].m_size = 2;subIds[count].m_scale = 2; break;
                case 15: subIds[count].m_value = g_txCalibDataCache.m_FlowValue; subIds[count].m_size = 2;subIds[count].m_scale = 2; break;
                case 16: subIds[count].m_value = g_txCalibDataCache.m_O2ValveAd; subIds[count].m_size = 2;subIds[count].m_scale = 0; break;
                case 17: subIds[count].m_value = g_txCalibDataCache.m_O2ValveFlow; subIds[count].m_size = 2;subIds[count].m_scale = 2; break;
                case 18: subIds[count].m_value = g_txCalibDataCache.m_MotorDutyCycle; subIds[count].m_size = 2;subIds[count].m_scale = 1; break;
                case 19: subIds[count].m_value = g_txCalibDataCache.m_TotalFlow; subIds[count].m_size = 2;subIds[count].m_scale = 2; break;
                case 20: subIds[count].m_value = g_txCalibDataCache.m_O2Flow; subIds[count].m_size = 2;subIds[count].m_scale = 2; break;
                case 21: subIds[count].m_value = g_txCalibDataCache.m_AirO2MixCoeff; subIds[count].m_size = 2;subIds[count].m_scale = 2; break;
            }
            g_txCalibDataCache.m_valid[i] = false;
            count++;
        }
    }
    uint16_t SendLen = ProtocolCreateSubIdData(TxData,PROTOCOL_ADDR_VCM_TO_MCM,false,
                                                PROTOCOL_TX_MID_CALIB_DATA,(uint8_t *)subIds,count);
    if (SendLen > 0) {
        ProtocolSendData(instance, PROTOCOL_PRIORITY_HIGH, TxData, SendLen);
    }
}


void ProtocolCalibDataProcess(uint8_t instance, uint32_t taskCounter)
{
#if PROTOCOL_CALIBDATA_SEND_ENABLE == 1
    MCalibFunMgrTxPacket_t *calibTxPacket;
    CalibrationType calibType;
    static bool isCalibInProgress = false;

    /* 定时发送校准数据 */
    if(taskCounter%200 == 0) {
        calibTxPacket = MCalibFunMgrGetTxPacket();
        if(calibTxPacket != NULL) {
            calibType = calibTxPacket->currentState;
            g_txCalibDataCache.m_currentCalibItem = (uint8_t)calibType;
            g_txCalibDataCache.m_valid[0x01] = true;
            switch(calibType) {
                case CALIB_ZERO:
                    if(calibTxPacket->zeroCalibTxCache->HistoryDataSendFlag == true) {
                        g_txCalibDataCache.m_inspPressureAdZeroHist = calibTxPacket->zeroCalibTxCache->m_InspPressValMem;
                        g_txCalibDataCache.m_expPressureAdZeroHist = calibTxPacket->zeroCalibTxCache->m_ExpPressValMem;
                        g_txCalibDataCache.m_peepPressureAdZeroHist = calibTxPacket->zeroCalibTxCache->m_PeepPressValMem;
                        g_txCalibDataCache.m_proxFlowAdZeroHist = calibTxPacket->zeroCalibTxCache->m_ProxPressValMem;
                        g_txCalibDataCache.m_valid[0x04] = true;
                        g_txCalibDataCache.m_valid[0x05] = true;
                        g_txCalibDataCache.m_valid[0x06] = true;
                        g_txCalibDataCache.m_valid[0x07] = true;
                        calibTxPacket->zeroCalibTxCache->HistoryDataSendFlag = false;
                    }
                    if(calibTxPacket->zeroCalibTxCache->CurrentDataSendFlag == true) {
                        g_txCalibDataCache.m_inspPressureAd = calibTxPacket->zeroCalibTxCache->m_InspPressVal;
                        g_txCalibDataCache.m_expPressureAd = calibTxPacket->zeroCalibTxCache->m_ExpPressVal;
                        g_txCalibDataCache.m_peepPressureAd = calibTxPacket->zeroCalibTxCache->m_PeepPressVal;
                        g_txCalibDataCache.m_proxFlowAd = calibTxPacket->zeroCalibTxCache->m_ProxPressVal;
                        g_txCalibDataCache.m_valid[0x08] = true;
                        g_txCalibDataCache.m_valid[0x09] = true;
                        g_txCalibDataCache.m_valid[0x0A] = true;
                        g_txCalibDataCache.m_valid[0x0B] = true;
                        calibTxPacket->zeroCalibTxCache->CurrentDataSendFlag = false;
                    }
                    if(calibTxPacket->zeroCalibTxCache->CalibResultSendFlag == true) {
                        g_txCalibDataCache.m_calibrationResult = (uint8_t)calibTxPacket->zeroCalibTxCache->m_CalibResult;
                        g_txCalibDataCache.m_valid[0x02] = true;
                        calibTxPacket->zeroCalibTxCache->CalibResultSendFlag = false;
                    }
                    if(calibTxPacket->zeroCalibTxCache->CalibErrorSendFlag == true) {
                        g_txCalibDataCache.m_faultCode = (uint8_t)calibTxPacket->zeroCalibTxCache->m_ErrorCode;
                        g_txCalibDataCache.m_valid[0x03] = true;
                        calibTxPacket->zeroCalibTxCache->CalibErrorSendFlag = false;
                    }
                    break;
                case CALIB_PRESSURE_SENSOR:
                    if(calibTxPacket->pressCalibTxCache->CalibResultSendFlag == true) {
                        g_txCalibDataCache.m_calibrationResult = (uint8_t)calibTxPacket->pressCalibTxCache->m_CalibResult;
                        g_txCalibDataCache.m_valid[0x02] = true;
                        calibTxPacket->pressCalibTxCache->CalibResultSendFlag = false;
                    }
                    if(calibTxPacket->pressCalibTxCache->CalibErrorSendFlag == true) {
                        g_txCalibDataCache.m_faultCode = (uint8_t)calibTxPacket->pressCalibTxCache->m_ErrorCode;
                        g_txCalibDataCache.m_valid[0x03] = true;
                        calibTxPacket->pressCalibTxCache->CalibErrorSendFlag = false;
                    }
                    if(calibTxPacket->pressCalibTxCache->CalibDataSendFlag == true) {
                        g_txCalibDataCache.m_TestIndex = calibTxPacket->pressCalibTxCache->m_TestIndex;
                        g_txCalibDataCache.m_MotorSpeed = calibTxPacket->pressCalibTxCache->m_TestSpeed;
                        g_txCalibDataCache.m_inspPressureAd = calibTxPacket->pressCalibTxCache->m_InspPressVal;
                        g_txCalibDataCache.m_expPressureAd = calibTxPacket->pressCalibTxCache->m_ExpPressVal;
                        g_txCalibDataCache.m_peepPressureAd = calibTxPacket->pressCalibTxCache->m_PeepPressVal;
                        g_txCalibDataCache.m_PressureValue = calibTxPacket->pressCalibTxCache->m_cmH2O;
                        g_txCalibDataCache.m_valid[0x08] = true;
                        g_txCalibDataCache.m_valid[0x09] = true;
                        g_txCalibDataCache.m_valid[0x0A] = true;
                        g_txCalibDataCache.m_valid[0x0C] = true;
                        g_txCalibDataCache.m_valid[0x0D] = true;
                        g_txCalibDataCache.m_valid[0x0E] = true;
                        calibTxPacket->pressCalibTxCache->CalibDataSendFlag = false;
                    }
                    if(calibTxPacket->pressCalibTxCache->CalibInProgressSendFlag == true) {
                        //处理中
                        g_txCalibDataCache.m_sendProgress = calibTxPacket->pressCalibTxCache->m_Progress;
                        g_txCalibDataCache.m_valid[0x00] = true;
                        calibTxPacket->pressCalibTxCache->CalibInProgressSendFlag = false;
                    }
                    break;
                case CALIB_PROXIMAL_FLOW_SENSOR:
                    if(calibTxPacket->proxFlowCalibTxCache->CalibResultSendFlag == true) {
                        g_txCalibDataCache.m_calibrationResult = (uint8_t)calibTxPacket->proxFlowCalibTxCache->m_CalibResult;
                        g_txCalibDataCache.m_valid[0x02] = true;
                        calibTxPacket->proxFlowCalibTxCache->CalibResultSendFlag = false;
                    }
                    if(calibTxPacket->proxFlowCalibTxCache->CalibErrorSendFlag == true) {
                        g_txCalibDataCache.m_faultCode = (uint8_t)calibTxPacket->proxFlowCalibTxCache->m_ErrorCode;
                        g_txCalibDataCache.m_valid[0x03] = true;
                        calibTxPacket->proxFlowCalibTxCache->CalibErrorSendFlag = false;
                    }
                    if(calibTxPacket->proxFlowCalibTxCache->CalibDataSendFlag == true) {
                        g_txCalibDataCache.m_TestIndex = calibTxPacket->proxFlowCalibTxCache->m_TestIndex;
                        g_txCalibDataCache.m_MotorSpeed = calibTxPacket->proxFlowCalibTxCache->m_TestSpeed;
                        g_txCalibDataCache.m_proxFlowAd = calibTxPacket->proxFlowCalibTxCache->m_ProxFlowAd;
                        g_txCalibDataCache.m_FlowValue = calibTxPacket->proxFlowCalibTxCache->m_ProxFlowStd;
                        g_txCalibDataCache.m_valid[0x0B] = true;
                        g_txCalibDataCache.m_valid[0x0C] = true;
                        g_txCalibDataCache.m_valid[0x0D] = true;
                        g_txCalibDataCache.m_valid[0x0F] = true;
                        calibTxPacket->proxFlowCalibTxCache->CalibDataSendFlag = false;
                    }
                    if(calibTxPacket->proxFlowCalibTxCache->CalibInProgressSendFlag == true) {
                        //处理中
                        g_txCalibDataCache.m_sendProgress = calibTxPacket->proxFlowCalibTxCache->m_Progress;
                        g_txCalibDataCache.m_valid[0x00] = true;
                        calibTxPacket->proxFlowCalibTxCache->CalibInProgressSendFlag = false;
                    }
                    break;
                case CALIB_OXYGEN_RATIO_VALVE_FLOW_CURRENT:
                    if(calibTxPacket->o2VCalibTxCache->CalibResultSendFlag == true) {
                        g_txCalibDataCache.m_calibrationResult = (uint8_t)calibTxPacket->o2VCalibTxCache->m_CalibResult;
                        g_txCalibDataCache.m_valid[0x02] = true;
                        calibTxPacket->o2VCalibTxCache->CalibResultSendFlag = false;
                    }
                    if(calibTxPacket->o2VCalibTxCache->CalibErrorSendFlag == true) {
                        g_txCalibDataCache.m_faultCode = (uint8_t)calibTxPacket->o2VCalibTxCache->m_ErrorCode;
                        g_txCalibDataCache.m_valid[0x03] = true;
                        calibTxPacket->o2VCalibTxCache->CalibErrorSendFlag = false;
                    }
                    if(calibTxPacket->o2VCalibTxCache->CalibDataSendFlag == true) {
                        g_txCalibDataCache.m_O2ValveAd = calibTxPacket->o2VCalibTxCache->m_O2ValveAd;
                        g_txCalibDataCache.m_O2Flow = calibTxPacket->o2VCalibTxCache->m_O2ValveFlow;
                        g_txCalibDataCache.m_MotorDutyCycle = calibTxPacket->o2VCalibTxCache->m_TestDuty;
                        g_txCalibDataCache.m_TestIndex = calibTxPacket->o2VCalibTxCache->m_TestIndex;
                        g_txCalibDataCache.m_valid[0x0C] = true;
                        g_txCalibDataCache.m_valid[0x10] = true;
                        g_txCalibDataCache.m_valid[0x14] = true;
                        g_txCalibDataCache.m_valid[0x12] = true;
                        calibTxPacket->o2VCalibTxCache->CalibDataSendFlag = false;
                    }
                    if(calibTxPacket->o2VCalibTxCache->CalibInProgressSendFlag == true) {
                        //处理中
                        g_txCalibDataCache.m_sendProgress = calibTxPacket->o2VCalibTxCache->m_Progress;
                        g_txCalibDataCache.m_valid[0x00] = true;
                        calibTxPacket->o2VCalibTxCache->CalibInProgressSendFlag = false;
                    }
                    break;
                case CALIB_AIR_OXYGEN_MIX_COEFFICIENT:
                    if(calibTxPacket->mixCalibTxCache->CalibResultSendFlag == true) {
                        g_txCalibDataCache.m_calibrationResult = (uint8_t)calibTxPacket->mixCalibTxCache->m_CalibResult;
                        g_txCalibDataCache.m_valid[0x02] = true;
                        calibTxPacket->mixCalibTxCache->CalibResultSendFlag = false;
                    }
                    if(calibTxPacket->mixCalibTxCache->CalibErrorSendFlag == true) {
                        g_txCalibDataCache.m_faultCode = (uint8_t)calibTxPacket->mixCalibTxCache->m_ErrorCode;
                        g_txCalibDataCache.m_valid[0x03] = true;
                        calibTxPacket->mixCalibTxCache->CalibErrorSendFlag = false;
                    }
                    if(calibTxPacket->mixCalibTxCache->CalibInProgressSendFlag == true) {
                        //处理中
                        g_txCalibDataCache.m_sendProgress = calibTxPacket->mixCalibTxCache->m_Progress;
                        g_txCalibDataCache.m_valid[0x00] = true;
                        calibTxPacket->mixCalibTxCache->CalibInProgressSendFlag = false;
                    }
                    if(calibTxPacket->mixCalibTxCache->CalibDataSendFlag == true) {
                        g_txCalibDataCache.m_TestIndex = calibTxPacket->mixCalibTxCache->m_TestIndex;
                        g_txCalibDataCache.m_O2Flow = calibTxPacket->mixCalibTxCache->m_O2Flow;
                        g_txCalibDataCache.m_TotalFlow = calibTxPacket->mixCalibTxCache->m_TotalFlow;
                        g_txCalibDataCache.m_AirO2MixCoeff = calibTxPacket->mixCalibTxCache->m_AirO2MixCoeff;
                        g_txCalibDataCache.m_valid[0x0C] = true;
                        g_txCalibDataCache.m_valid[0x13] = true;
                        g_txCalibDataCache.m_valid[0x14] = true;
                        g_txCalibDataCache.m_valid[0x15] = true;

                        calibTxPacket->mixCalibTxCache->CalibDataSendFlag = false;
                    }
                    break;
                default:
                    break;
            }
            if(isCalibInProgress == false) {
                g_txCalibDataCache.m_valid[0x00] = false;
            }
            for(uint8_t i=0;i<PROTOCOL_CALIB_DATA_SUBID_MAX;i++) {
                if(g_txCalibDataCache.m_valid[i] !=0 && i!= 0x01) {
                    ProtocolCalibDataSend(instance);
                }
            }
        }
        
    }
#endif
}

void ProtocolSpecialDataSend(uint8_t instance)
{
    // 发送数据
    uint8_t TxData[256];
    SubIDCache_t subIds[PROTOCOL_SPECIAL_FUNC_SUBID_MAX];
    uint8_t count = 0;

    for (uint8_t i = 0; i < PROTOCOL_SPECIAL_FUNC_SUBID_MAX; i++) {
        if (g_txSpecialFuncCache.m_valid[i]) {
            subIds[count].m_id = i;
            subIds[count].m_valid = 1;

            switch (i) {
                case 0x00: subIds[count].m_value = g_txSpecialFuncCache.m_manual;      subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                case 0x01: subIds[count].m_value = g_txSpecialFuncCache.m_inspHold;    subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                case 0x02: subIds[count].m_value = g_txSpecialFuncCache.m_exhHold;     subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                case 0x03: subIds[count].m_value = g_txSpecialFuncCache.m_o2Boost;     subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                case 0x04: subIds[count].m_value = g_txSpecialFuncCache.m_si;          subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                case 0x05: subIds[count].m_value = g_txSpecialFuncCache.m_sbt;         subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                case 0x06: subIds[count].m_value = g_txSpecialFuncCache.m_o2Therapy;   subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                case 0x07: subIds[count].m_value = g_txSpecialFuncCache.m_cprv;        subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                case 0x08: subIds[count].m_value = g_txSpecialFuncCache.m_o2ConsTool;  subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                case 0x09: subIds[count].m_value = g_txSpecialFuncCache.m_P01;         subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                case 0x0A: subIds[count].m_value = g_txSpecialFuncCache.m_nif;         subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                case 0x0B: subIds[count].m_value = g_txSpecialFuncCache.m_peepi;       subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
				case 0x0C: subIds[count].m_value = g_txSpecialFuncCache.m_pv;          subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                case 0x0D: subIds[count].m_value = g_txSpecialFuncCache.m_cprVent;     subIds[count].m_size = 1; subIds[count].m_scale = 0; break;
                default:
                    subIds[count].m_value = 0;
                    subIds[count].m_size = 1;
                    subIds[count].m_scale = 0;
                    break;
            }
            g_txSpecialFuncCache.m_valid[i] = false;
            count++;
        }
    }

    uint16_t SendLen = ProtocolCreateSubIdData(TxData, PROTOCOL_ADDR_VCM_TO_MCM, false,
                                              PROTOCOL_TX_MID_SPECIAL_FUNC, (uint8_t *)subIds, count);
    if (SendLen > 0) {
        ProtocolSendData(instance, PROTOCOL_PRIORITY_HIGH, TxData, SendLen);
    }
}

void ProtocolSpecialDataProcess(uint8_t instance, uint32_t taskCounter)
{
    static TxSpecialFuncCache_t txSpecialMemory;
    uint8_t tmpStatus;
    if (taskCounter % 50 == 0) { // 每50ms处理一次
        for (int i = 0; i < PROTOCOL_SPECIAL_FUNC_SUBID_MAX; i++) {
            switch(i) {
                case 0x00: 
                    txSpecialMemory.m_manual = (uint8_t)iVentMonitParamsGet(VENT_MNT_CTRL_MANUAL_VENT_EXEC_STATE);
                    if(txSpecialMemory.m_manual != g_txSpecialFuncCache.m_manual) {
                        g_txSpecialFuncCache.m_manual = txSpecialMemory.m_manual;
                        g_txSpecialFuncCache.m_valid[0x00] = true;
                    }
                    break;
                case 0x01: 
                    txSpecialMemory.m_inspHold = (uint8_t)iVentMonitParamsGet(VENT_MNT_CTRL_INSP_HOLD_EXEC_STATE);
                    if(txSpecialMemory.m_inspHold != g_txSpecialFuncCache.m_inspHold) {
                        g_txSpecialFuncCache.m_inspHold = txSpecialMemory.m_inspHold;
                        g_txSpecialFuncCache.m_valid[0x01] = true;
                    }
                    break;
                case 0x02: 
                    txSpecialMemory.m_exhHold = (uint8_t)iVentMonitParamsGet(VENT_MNT_CTRL_EXP_HOLD_EXEC_STATE);
                    if(txSpecialMemory.m_exhHold != g_txSpecialFuncCache.m_exhHold) {
                        g_txSpecialFuncCache.m_exhHold = txSpecialMemory.m_exhHold;
                        g_txSpecialFuncCache.m_valid[0x02] = true;
                    }
                    break;
                case 0x03: 
                    txSpecialMemory.m_o2Boost = (uint8_t)iVentMonitParamsGet( VENT_MNT_TOOL_O2AUG_STATE);
                    if(txSpecialMemory.m_o2Boost != g_txSpecialFuncCache.m_o2Boost) {
                        g_txSpecialFuncCache.m_o2Boost = txSpecialMemory.m_o2Boost;
                        g_txSpecialFuncCache.m_valid[0x03] = true;
                    }
                    break;
                case 0x04: 
                    txSpecialMemory.m_si = (uint8_t)iVentMonitParamsGet(VENT_MNT_CTRL_SI_TOOL_EXEC_STATE);
                    if(txSpecialMemory.m_si != g_txSpecialFuncCache.m_si) {
                        g_txSpecialFuncCache.m_si = txSpecialMemory.m_si;
                        g_txSpecialFuncCache.m_valid[0x04] = true;
                    }
                    break;
                case 0x05: 
                    txSpecialMemory.m_sbt = (uint8_t)iVentMonitParamsGet(VENT_MNT_CTRL_SBT_EXEC_STATE);
                    if(txSpecialMemory.m_sbt != g_txSpecialFuncCache.m_sbt) {
                        g_txSpecialFuncCache.m_sbt = txSpecialMemory.m_sbt;
                        g_txSpecialFuncCache.m_valid[0x05] = true;
                    }
                    break;
                case 0x06: 
                    // txSpecialMemory.m_o2Therapy = (uint8_t)iVentMonitParamsGet(VENT_MNT_CTRL_O2_THERAPY_EXEC_ENABLE);
                    // if(txSpecialMemory.m_o2Therapy != g_txSpecialFuncCache.m_o2Therapy) {
                    //     g_txSpecialFuncCache.m_o2Therapy = txSpecialMemory.m_o2Therapy;
                    //     g_txSpecialFuncCache.m_valid[0x06] = true;
                    // }
                    break;
                case 0x07: 
                    // txSpecialMemory.m_cprv = (uint8_t)iVentMonitParamsGet(VENT_MNT_CTRL_CPRV_EXEC_ENABLE);
                    // if(txSpecialMemory.m_cprv != g_txSpecialFuncCache.m_cprv) {
                    //     g_txSpecialFuncCache.m_cprv = txSpecialMemory.m_cprv;
                    //     g_txSpecialFuncCache.m_valid[0x07] = true;
                    // }
                    break;
                case 0x08: 
                    // txSpecialMemory.m_o2ConsTool = (uint8_t)iVentMonitParamsGet(VENT_MNT_CTRL_O2_CONS_TOOL_EXEC_ENABLE);
                    // if(txSpecialMemory.m_o2ConsTool != g_txSpecialFuncCache.m_o2ConsTool) {
                    //     g_txSpecialFuncCache.m_o2ConsTool = txSpecialMemory.m_o2ConsTool;
                    //     g_txSpecialFuncCache.m_valid[0x08] = true;
                    // }
                    break;
                case 0x09: 
                    txSpecialMemory.m_P01 = (uint8_t)iVentMonitParamsGet(VENT_MNT_CTRL_P01_TOOL_EXEC_STATE);
                    if(txSpecialMemory.m_P01 != g_txSpecialFuncCache.m_P01) {
                        g_txSpecialFuncCache.m_P01 = txSpecialMemory.m_P01;
                        g_txSpecialFuncCache.m_valid[0x09] = true;
                    }
                    break;
                case 0x0A: 
                    txSpecialMemory.m_nif = (uint8_t)iVentMonitParamsGet(VENT_MNT_CTRL_NIF_TOOL_EXEC_STATE);
                    if(txSpecialMemory.m_nif != g_txSpecialFuncCache.m_nif) {
                        g_txSpecialFuncCache.m_nif = txSpecialMemory.m_nif;
                        g_txSpecialFuncCache.m_valid[0x0A] = true;
                    }
                    break;
                case 0x0B: 
                    txSpecialMemory.m_peepi = (uint8_t)iVentMonitParamsGet(VENT_MNT_CTRL_PEEPI_TOOL_EXEC_STATE);
                    if(txSpecialMemory.m_peepi != g_txSpecialFuncCache.m_peepi) {
                        g_txSpecialFuncCache.m_peepi = txSpecialMemory.m_peepi;
                        g_txSpecialFuncCache.m_valid[0x0B] = true;
                    }
                    break;
                case 0x0C: 
                    txSpecialMemory.m_pv = (uint8_t)iVentMonitParamsGet(VENT_MNT_CTRL_PV_TOOL_EXEC_STATE);
                    if(txSpecialMemory.m_pv != g_txSpecialFuncCache.m_pv) {
                        g_txSpecialFuncCache.m_pv = txSpecialMemory.m_pv;
                        g_txSpecialFuncCache.m_valid[0x0C] = true;
                    }
                    break;
                case 0x0D:
                    //txSpecialMemory.m_cprVent = ;
                    if(txSpecialMemory.m_cprVent != g_txSpecialFuncCache.m_cprVent) {
                        g_txSpecialFuncCache.m_cprVent = txSpecialMemory.m_cprVent;
                        g_txSpecialFuncCache.m_valid[0x0D] = true;
                    }
                    break;
                default:
                    tmpStatus = 0;
                    break;
            }
        }
        for(int i=0;i<PROTOCOL_SPECIAL_FUNC_SUBID_MAX;i++) {
            if(g_txSpecialFuncCache.m_valid[i] == true) {
                ProtocolSpecialDataSend(instance);
                return;
            }
        }
    }
}

static void ProtocolVersionDataSend(uint8_t instance)
{
    uint8_t TxData[256];
    SubIDCache_t subIds[PROTOCOL_VERSION_INFO_SUBID_MAX];
    uint8_t count = 0;

    for (uint8_t i = 0; i < PROTOCOL_VERSION_INFO_SUBID_MAX; i++) {
        if (g_txVersionInfoCache.m_valid[i]) {
            subIds[count].m_id = i;
            subIds[count].m_valid = true;

            switch (i) {
                case 0x00:
                    subIds[count].m_value = g_txVersionInfoCache.m_swVersion;
                    subIds[count].m_size = 4;
                    subIds[count].m_scale = 0;
                    break;
                case 0x01:
                    subIds[count].m_value = g_txVersionInfoCache.m_hwVersion;
                    subIds[count].m_size = 2;
                    subIds[count].m_scale = 0;
                    break;
                default:
                    subIds[count].m_value = 0;
                    subIds[count].m_size = 1;
                    subIds[count].m_scale = 0;
                    break;
            }

            g_txVersionInfoCache.m_valid[i] = false;
            count++;
        }
    }

    if (count > 0) {
        uint16_t SendLen = ProtocolCreateSubIdData(TxData, PROTOCOL_ADDR_VCM_TO_MCM, false,
                                                  PROTOCOL_TX_MID_VERSION_INFO, (uint8_t *)subIds, count);
        if (SendLen > 0) {
            ProtocolSendData(instance, PROTOCOL_PRIORITY_HIGH, TxData, SendLen);
        }
    }
}

void ProtocolVersionDataProcess(uint8_t instance, uint32_t taskCounter)
{
#if PROTOCOL_VERSION_SEND_ENABLE == 1
    if (g_rxDataQueryCache.m_valid[0x00] == false) {
        return;
    }
    g_rxDataQueryCache.m_valid[0x00] = false;

    g_txVersionInfoCache.m_swVersion = PROTOCOL_VCM_SW_VERSION_VALUE;
    g_txVersionInfoCache.m_hwVersion = PROTOCOL_VCM_HW_VERSION_VALUE;
    g_txVersionInfoCache.m_valid[0x00] = true;
    g_txVersionInfoCache.m_valid[0x01] = true;

    ProtocolVersionDataSend(instance);
#endif  
}


#endif
void ProtocolDataPreProcess(uint8_t instance)
{
    static uint32_t taskCounter = 0;
    taskCounter += PROTOCOL_TASK_DELAY_MS;
    MCMConnectedCounter += PROTOCOL_TASK_DELAY_MS;
	
    /*定时采集波形数据*/
    ProtocolWaveDataProcess(instance, taskCounter);

    /*定时处理心跳*/
    ProtocolHeartbeatDataProcess(instance, taskCounter);

    /*定时处理监测参数*/
    ProtocolDetectDataPreProcess(instance, taskCounter);

    /*定时处理生理报警*/
    // ProtocolPhysAlarmDataProcess(instance, taskCounter); /* Pending machine port. */

    /*定时处理技术报警*/
    // ProtocolTechAlarmDataProcess(instance, taskCounter); /* Pending machine port. */

    /*定时处理自检数据*/
    // ProtocolSelfTestDataProcess(instance, taskCounter); /* Pending machine port. */
    
    /*定时处理诊断数据*/
    // ProtocolDiagnosesDataProcess(instance, taskCounter); /* Pending machine port. */

    /*定时处理校准数据*/
    // ProtocolCalibDataProcess(instance, taskCounter); /* Pending machine port. */

    /*定时处理特殊工具数据*/
    // ProtocolSpecialDataProcess(instance, taskCounter); /* Pending machine port. */

    /*处理版本号信息*/
    // ProtocolVersionDataProcess(instance, taskCounter); /* Pending machine port. */



   if(MCMConnectedCounter >= PROTOCOL_MCM_DISCONNECT_TIMEOUT_MS) {
       MCMConnectedCounter = PROTOCOL_MCM_DISCONNECT_TIMEOUT_MS; // 防止溢出
   }
}

bool ProtocolIsMCMConnected(void)
{
    return isMCMOnline && (MCMConnectedCounter < PROTOCOL_MCM_DISCONNECT_TIMEOUT_MS);
}

/* ==================== 协议缓存访问函数 ==================== */
const RxVentParamsCache_t* ProtocolGetRxVentParamsCache(void)
{
    return &g_rxVentParamsCache;
}

const RxVentSwitchCache_t* ProtocolGetRxVentSwitchCache(void)
{
#if 0 /* Legacy manual-breath completion detection. */
    E_RESP_PERIOD_TYPE CurrtRespPeriod = (E_RESP_PERIOD_TYPE)iVentMonitParamsGet(VENT_MNT_CTRL_RESP_PERIOD) ;
    E_EXP_PHASE_EXIT_TYPE LastExpPhaseExitType = (E_EXP_PHASE_EXIT_TYPE)iVentMonitParamsGet(VENT_MNT_CTRL_EXP_PHASE_EXIT) ;

    // Manual breath
    if (g_rxVentSwitchCache.m_valid[0x03]) {
        if (CurrtRespPeriod == RESP_PERIOD_INSP_SPT && LastExpPhaseExitType == EXP_PHASE_MVENT_EXIT)
        {
            g_rxVentSwitchCache.m_manualBreath = false ;
        }
    }
	
#endif
    return &g_rxVentSwitchCache;
}

const RxSystemMenuCache_t* ProtocolGetRxSystemMenuCache(void)
{
    return &g_rxSystemMenuCache;
}

const RxManufacturerCache_t* ProtocolGetRxManufacturerCache(void)
{
    return &g_rxManufacturerCache;
}

const RxAlarmLimitsCache_t* ProtocolGetRxAlarmLimitsCache(void)
{
    return &g_rxAlarmLimitsCache;
}

const RxSelfTestCache_t* ProtocolGetRxSelfTestCache(void)
{
    return &g_rxSelfTestCache;
}

const SelfCheckData_t* ProtocolGetSelfTestInfoCache(void)
{
    return &SystemTest;
}

const RxDiagnosisCache_t* ProtocolGetRxDiagnosisCache(void)
{
    return &g_rxDiagnosisCache;
}

const RxCalibrationCache_t* ProtocolGetRxCalibDataCache(void)
{
    return &g_rxCalibrationCache;
}

/**************************End of file********************************/
