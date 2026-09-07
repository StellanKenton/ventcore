/**
**********************************************************************************
* @file     : ProtoclOfVcm.h
* @brief    : VCM相关的通信协议头文件
* @details  : 该文件包含VCM相关的通信协议函数的声明
* @author   : \.
* @date     : 2025-01-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************
*/
#ifndef PROTOCOL_VCM_H
#define PROTOCOL_VCM_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif
#include "ProtoclOfProcess.h"
#include "settingdata.h"
/* ====================================================== 项目相关定义 ====================================================== */
// - Boot 起始地址：`0x08000000`
// - Boot 空间大小：`0x0000C000`（Sector 0 ~ Sector 2，共 48 KB）
// - 标志位空间起始地址：`0x0800C000`
// - 标志位空间大小：`0x00004000`（Sector 3，共 16 KB）
// - APP 起始地址：`0x08010000`
#define PROTOCOL_UPGRADE_FLAG_FLASH_ADDR   ((uint32_t)0x0800C000UL)
#define PROTOCOL_UPGRADE_FLAG_FLASH_SECTOR FLASH_Sector_3
#define PROTOCOL_UPGRADE_FLAG_FLASH_RANGE  VoltageRange_3
#define PROTOCOL_UPGRADE_FLAG_SET_VALUE    ((uint8_t)0x01U)
#define PROTOCOL_UPGRADE_FLAG_CLEAR_VALUE  ((uint8_t)0x00U)
#define PROTOCOL_FLASH_CLEAR_ALL_FLAGS     (FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR)
#define UPGRADE_CODE               (0xACBDEF11U)
#define UPGRADE_FLAG_ERASE_VALUE   (0xFFFFFFFFU)

/* ==================== 版本信息定义 ==================== */
#define PROTOCOL_VCM_SW_VERSION_MAJOR      ((uint8_t)0U)
#define PROTOCOL_VCM_SW_VERSION_MINOR      ((uint8_t)0U)
#define PROTOCOL_VCM_SW_VERSION_PATCH      ((uint8_t)0U)
#define PROTOCOL_VCM_SW_VERSION_BUILD      ((uint8_t)19U)

#define PROTOCOL_VCM_HW_VERSION_MAJOR      ((uint8_t)0U)
#define PROTOCOL_VCM_HW_VERSION_MINOR      ((uint8_t)1U)

#define PROTOCOL_VCM_SW_VERSION_VALUE      \
    (((uint32_t)PROTOCOL_VCM_SW_VERSION_MAJOR << 24) | \
     ((uint32_t)PROTOCOL_VCM_SW_VERSION_MINOR << 16) | \
     ((uint32_t)PROTOCOL_VCM_SW_VERSION_PATCH << 8) | \
     (uint32_t)PROTOCOL_VCM_SW_VERSION_BUILD)

#define PROTOCOL_VCM_HW_VERSION_VALUE      \
    (((uint16_t)PROTOCOL_VCM_HW_VERSION_MAJOR << 8) | \
     (uint16_t)PROTOCOL_VCM_HW_VERSION_MINOR)

/* ==================== 数据发送开启定义 ==================== */
#define PROTOCOL_WAVE_DATA_SEND_ENABLE         1        /* 通气波形数据发送开关 */
#define PROTOCOL_MONITOR_PARAMS_SEND_ENABLE    0        /* 通气监测参数发送开关 */
#define PROTOCOL_HEARTBEAT_SEND_ENABLE         1        /* 心跳发送开关 */
#define PROTOCOL_TECHALARM_SEND_ENABLE         0        /* 技术报警发送开关 */
#define PROTOCOL_SELFTEST_SEND_ENABLE          0        /* 自检发送开关 */
#define PROTOCOL_DIAGNOSES_SEND_ENABLE         0        /* 诊断数据发送开关 */
#define PROTOCOL_CALIBDATA_SEND_ENABLE         0        /* 校准数据发送开关 */
#define PROTOCOL_VERSION_SEND_ENABLE           0        /* 版本信息发送开关 */

#define PROTOCOL_MCM_DISCONNECT_TIMEOUT_MS        5000   /* MCM断连超时时间，单位：毫秒 */
/* VentTask only; cache publication and settings copies use RTOS critical sections. */
void protocolApplyReceivedSettings(void);

typedef struct stProtocolHeartbeatStats {
    uint32_t received;
    uint32_t transmitted; /* Complete reply frames accepted by the UART transport. */
    uint32_t overflow;
    uint16_t pending; /* Replies not yet accepted by the protocol TX queue. */
} stProtocolHeartbeatStats;

/* CommTask only: diagnostics and notification after UART accepts a heartbeat reply. */
void protocolHeartbeatStatsGet(stProtocolHeartbeatStats *stats);
void protocolHeartbeatTransmitted(void);

/* ==================== 协议主ID定义 ==================== */
/* 帧头地址定义 */
#define PROTOCOL_ADDR_MCM_TO_VCM        0xFFFE    /* MCM -> VCM */
#define PROTOCOL_ADDR_VCM_TO_MCM        0xFEFF    /* VCM -> MCM */

/* RX (MCM -> VCM) 主ID定义 */
#define PROTOCOL_RX_MID_HEARTBEAT       0x7F      /* 心跳包 */
#define PROTOCOL_RX_MID_VENT_PARAMS     0xAF      /* 通气参数设置 */
#define PROTOCOL_RX_MID_VENT_SWITCH     0xAE      /* 通气开关设置 */
#define PROTOCOL_RX_MID_SYSTEM_MENU     0xAD      /* 系统菜单设置 */
#define PROTOCOL_RX_MID_ALARM_LIMITS    0xAC      /* 报警限设置 */
#define PROTOCOL_RX_MID_SELF_TEST       0xAB      /* 自检设置 */
#define PROTOCOL_RX_MID_CALIBRATION     0xAA      /* 校准设置 */
#define PROTOCOL_RX_MID_DIAGNOSIS       0xA9      /* 诊断设置 */
#define PROTOCOL_RX_MID_DATA_QUERY      0xA8      /* 数据查询设置 */
#define PROTOCOL_RX_MID_MANUFACTURER    0xA7      /* 厂家配置设置 */
#define PROTOCOL_RX_MID_UPGRADE         0xA6      /* 升级设置 */

/* TX (VCM -> MCM) 主ID定义 */
#define PROTOCOL_TX_MID_HEARTBEAT       0x7F      /* 心跳包 */
#define PROTOCOL_TX_MID_WAVE_DATA       0xAF      /* 发送通气波形数据 */
#define PROTOCOL_TX_MID_MONITOR_PARAMS  0xAE      /* 发送通气监测参数 */
#define PROTOCOL_TX_MID_PHYS_ALARM      0xAD      /* 发送生理报警信息 */
#define PROTOCOL_TX_MID_PHYS_ALARM_MSG  0xAC      /* 发送生理报警提示信息 */
#define PROTOCOL_TX_MID_TECH_ALARM      0xAB      /* 发送技术报警信息 */
#define PROTOCOL_TX_MID_TECH_ALARM_MSG  0xAA      /* 发送技术报警提示信息 */
#define PROTOCOL_TX_MID_SPECIAL_FUNC    0xA9      /* 发送特殊功能状态信息 */
#define PROTOCOL_TX_MID_CALIB_DATA      0xA8      /* 发送校准数据信息 */
#define PROTOCOL_TX_MID_DIAG_DATA       0xA7      /* 诊断数据信息 */
#define PROTOCOL_TX_MID_RESULT_MSG      0xA6      /* 发送提示&结果信息 */
#define PROTOCOL_TX_MID_ERROR_REQUEST   0xA5      /* 发送VCM异常请求指令 */
#define PROTOCOL_TX_MID_VERSION_INFO    0xA4      /* 发送版本信息 */
#define PROTOCOL_TX_MID_UPGRADE_INFO    0xA3      /* 发送升级信息 */

/* ==================== 接收主ID缓存结构体定义 ==================== */


/* 通气参数缓存 (0xAF) */
#define PROTOCOL_VENT_PARAMS_SUBID_MAX    67      /* 通气参数子ID最大数量，支持到子ID 0x42 */
typedef struct {
    uint8_t m_ventType;             /* 0x00 - 通气类型 */
    uint8_t m_ventType_scale;       
    eVentMode m_mode;        /* 0x01 - 通气模式 */
    eVentMode m_modeRecv;
    uint8_t m_mode_scale;           
    uint8_t m_patientType;          /* 0x02 - 病人类型 */
    uint8_t m_patientType_scale;    
    uint8_t m_gender;               /* 0x03 - 性别 */
    uint8_t m_gender_scale;         
    uint8_t m_idealHeight;          /* 0x04 - 理想身高 */
    uint8_t m_idealHeight_scale;   
    uint16_t m_idealWeight;         /* 0x05 - 理想体重 */
    uint8_t m_idealWeight_scale;    
    uint8_t m_fio2;                 /* 0x06 - 氧浓度 */
    uint8_t m_fio2_scale;          
    uint8_t m_deltaPinsp;           /* 0x07 - △吸气压力 */
    uint8_t m_deltaPinsp_scale;     
    uint8_t m_peep;                 /* 0x08 - 呼末正压 */
    uint8_t m_peep_scale;           
    uint8_t m_deltaPsupp;           /* 0x09 - △支持压力 */
    uint8_t m_deltaPsupp_scale;     
    uint8_t m_pHigh;                /* 0x0A - 高水平压 */
    uint8_t m_pHigh_scale;         
    uint8_t m_pLow;                 /* 0x0B - 低水平压 */
    uint8_t m_pLow_scale;           
    uint8_t m_deltaApneaP;          /* 0x0C - △窒息压力 */
    uint8_t m_deltaApneaP_scale;    
    uint8_t m_deltaSighP;           /* 0x0D - △叹息压力 */
    uint8_t m_deltaSighP_scale;     
    uint16_t m_tidalVolume;         /* 0x0E - 潮气量 */
    uint8_t m_tidalVolume_scale;    
    uint16_t m_apneaTidalVolume;    /* 0x0F - 窒息潮气量 */
    uint8_t m_apneaTidalVolume_scale; 
    uint16_t m_flow;                 /* 0x10 - 流速 */
    uint8_t m_flow_scale;           
    uint16_t m_trigFlow;            /* 0x11 - 流速触发 */
    uint8_t m_trigFlow_scale;       
    int16_t m_trigPress;            /* 0x12 - 压力触发 */
    uint8_t m_trigPress_scale;      
    uint8_t m_exhTrigPercent;       /* 0x13 - 呼气触发 */
    uint8_t m_exhTrigPercent_scale; 
    uint8_t m_rate;                 /* 0x14 - 呼吸频率 */
    uint8_t m_rate_scale;           
    uint8_t m_simvRate;             /* 0x15 - SIMV频率 */
    uint8_t m_simvRate_scale;       
    uint8_t m_apneaRate;            /* 0x16 - 窒息频率 */
    uint8_t m_apneaRate_scale;      
    uint16_t m_ti;                  /* 0x17 - 吸气时间 */
    uint8_t m_ti_scale;            
    uint16_t m_tiMax;               /* 0x18 - 最大吸气时间 */
    uint8_t m_tiMax_scale;          
    uint16_t m_apneaTi;             /* 0x19 - 窒息吸气时间 */
    uint8_t m_apneaTi_scale;        
    uint16_t m_riseTime;            /* 0x1A - 上升时间 */
    uint8_t m_riseTime_scale;       
    uint16_t m_tHigh;               /* 0x1B - 高压时间 */
    uint8_t m_tHigh_scale;          
    uint16_t m_tLow;                /* 0x1C - 低压时间 */
    uint8_t m_tLow_scale;           
    uint8_t m_sighCount;            /* 0x1D - 叹息次数 */
    uint8_t m_sighCount_scale;      
    uint16_t m_sighIntervalMin;      /* 0x1E - 叹息间隔 */
    uint8_t m_sighIntervalMin_scale; 
    uint8_t m_siPress;              /* 0x1F - SI保持压力 */
    uint8_t m_siPress_scale;        
    uint8_t m_siTime;               /* 0x20 - SI保持时长 */
    uint8_t m_siTime_scale;         
    uint16_t m_sbtDuration;         /* 0x21 - SBT持续时间 */
    uint8_t m_sbtDuration_scale;    
    uint16_t m_sbtTolerance;        /* 0x22 - SBT耐受时间 */
    uint8_t m_sbtTolerance_scale;   
    uint8_t m_expTime;              /* 0x23 - 呼气时间 */
    uint8_t m_expTime_scale;
    uint8_t m_assistTrig;           /* 0x24 - 辅助触发开关 */
    uint8_t m_assistTrig_scale;
    uint8_t m_FlowTrigger;          /* 0x25 - 流速触发/压力触发 */
    uint8_t m_FlowTrigger_scale;
    uint8_t m_inspPausePercent;     /* 0x26 - 吸气暂停百分比 */
    uint8_t m_inspPausePercent_scale;
    uint16_t m_nCPAP;                /* 0x27 - 经鼻持续气道正压 (范围: 0~15.0 cmH2O) */
    uint8_t m_nCPAP_scale;
    uint16_t m_nIPAP;                /* 0x28 - 经鼻间歇气道正压 (范围: 0~15.0 cmH2O) */
    uint8_t m_nIPAP_scale;
    uint8_t m_peakPressure;         /* 0x29 - 最高压 (范围: 5~60 cmH2O) */
    uint8_t m_peakPressure_scale;
    uint16_t m_maxTidalVolume;      /* 0x2A - 最大潮气量 - 成人 (范围: 100~2500 mL) */
    uint8_t m_maxTidalVolume_scale;
    uint16_t m_maxVolumeAssist;     /* 0x2B - 最大容辅 (范围: 20~300 cmH2O/L) */
    uint8_t m_maxVolumeAssist_scale;
    uint8_t m_maxFlowAssist;        /* 0x2C - 最大流辅 (范围: 0~100 cmH2O/L) */
    uint8_t m_maxFlowAssist_scale;
    uint8_t m_breathSupportPercent; /* 0x2D - 呼吸支持% (范围: 0~100 %) */
    uint8_t m_breathSupportPercent_scale;
    uint16_t m_minuteVentilationPercent; /* 0x2E - 分钟通气量% (范围: 0~350 %) */
    uint8_t m_minuteVentilationPercent_scale;
    uint8_t m_pStart;                  /* 0x2F - Pstart (范围: 0~35 cmH2O) */
    uint8_t m_pStart_scale;
    uint8_t m_pMax;                    /* 0x30 - Pmax (范围: 25~65 cmH2O) */
    uint8_t m_pMax_scale;
    uint16_t m_o2TherapyFlow;          /* 0x31 -  氧疗流速 (2~80) */
    uint8_t m_o2TherapyFlow_scale;
    uint8_t m_peepEnd;                 /* 0x32 - PEEPend (范围: 0~35 cmH2O) */
    uint8_t m_peepEnd_scale;
    uint8_t m_ramp;                    /* 0x33 - Ramp (选项: OFF, 2~5) */
    uint8_t m_ramp_scale;
    uint8_t m_tPause;                  /* 0x34 - Tpause (范围: 0~30) */
    uint8_t m_tPause_scale;
    uint8_t m_tManeuver;               /* 0x35 - Tmaneuver (范围: 0~120) */
    uint8_t m_tManeuver_scale;
    uint8_t m_tHold;                   /* 0x36 - Thold (范围: 0~30 cmH2O) */
    uint8_t m_tHold_scale;
    uint8_t m_pHold;                   /* 0x37 - Phold (范围: 25~65 cmH2O) */
    uint8_t m_pHold_scale;
    uint8_t m_intub_type;         /* 0x38 - 插管类型 (0=气管内, 1=气管切开) */
    uint8_t m_intub_type_scale;
    uint8_t m_tube_diam;          /* 0x39 - 管径 */
    uint8_t m_tube_diam_scale;
    uint8_t m_comp_ratio;         /* 0x40 - 插管补偿比例 (0~100%) */
    uint8_t m_comp_ratio_scale;
    uint8_t m_auto_irc;           /* 0x41 - 自动插管阻力补偿 (0=OFF, 1=ON) */
    uint8_t m_exp_comp;           /* 0x42 - 插管呼气阶段补偿 (0=OFF, 1=ON) */

    bool m_valid[PROTOCOL_VENT_PARAMS_SUBID_MAX];               /* 有效性标志 */
} RxVentParamsCache_t;

/* 通气开关缓存 (0xAE) */
#define PROTOCOL_VENT_SWITCH_SUBID_MAX    16      /* 通气开关子ID最大数量 */
typedef struct {
    uint8_t m_command;              /* 0x00 - 通气命令 */
    uint8_t m_command_scale;        /* 通气命令缩放 */
    uint8_t m_sigh;                 /* 0x01 - 叹息 */
    uint8_t m_sigh_scale;           /* 叹息缩放 */
    uint8_t m_syncEnhance;          /* 0x02 - 同步增强 */
    uint8_t m_syncEnhance_scale;    /* 同步增强缩放 */
    uint8_t m_manualBreath;         /* 0x03 - 手动呼吸 */
    uint8_t m_manualBreath_scale;   /* 手动呼吸缩放 */
    uint8_t m_inspHold;             /* 0x04 - 吸气保持 */
    uint8_t m_inspHold_scale;       /* 吸气保持缩放 */
    uint8_t m_exhHold;              /* 0x05 - 呼气保持 */
    uint8_t m_exhHold_scale;        /* 呼气保持缩放 */
    uint8_t m_o2Boost;              /* 0x06 - 增氧 */
    uint8_t m_o2Boost_scale;        /* 增氧缩放 */
    uint8_t m_o2Boost2;             /* 0x07 - 二次增氧 */
    uint8_t m_o2Boost2_scale;       /* 二次增氧缩放 */
    uint8_t m_si;                   /* 0x08 - SI */
    uint8_t m_si_scale;             /* SI缩放 */
    uint8_t m_sbt;                  /* 0x09 - SBT */
    uint8_t m_sbt_scale;            /* SBT缩放 */
    uint8_t m_ApneaVentSwitch;      /* 0x0A 窒息通气开关 */
    uint8_t m_ApneaVentSwitch_scale;            
    uint8_t m_P01;                  /* 0x0B - P0.1 (吸气压力阈值) - 范围: 0~1 (0:停止, 1:开始) */
    uint8_t m_P01_scale;            
    uint8_t m_nif;                  /* 0x0C - NIF (最大吸气负压) - 范围: 0~1 (0:停止, 1:开始) */
    uint8_t m_nif_scale;            
    uint8_t m_peepi;                /* 0x0D - PEEPi (内源性PEEP) - 范围: 0~1 (0:停止, 1:开始) */
    uint8_t m_peepi_scale;          
    uint8_t m_p_v;                  /* 0x0E - P-V (压力-容积曲线) - 范围: 0~1 (0:停止, 1:开始) */
    uint8_t m_p_v_scale;            
    uint8_t m_cprVent;              /* 0x0F - CPR ventilation */
    uint8_t m_cprVent_scale;
    bool m_valid[PROTOCOL_VENT_SWITCH_SUBID_MAX];               /* 有效性标志 */
} RxVentSwitchCache_t;

/* 系统菜单缓存 (0xAD) */
#define PROTOCOL_SYSTEM_MENU_SUBID_MAX    11       /* 系统菜单子ID最大数量，支持到子ID 0x0A */
typedef struct {
    uint16_t m_ibwTidalVolume;      /* 0x00 - 理想体重潮气量 */
    uint8_t m_ibwTidalVolume_scale; /* 理想体重潮气量缩放 */
    uint8_t m_o2BoostFio2;          /* 0x01 - 增氧氧浓度 */
    uint8_t m_o2BoostFio2_scale;    /* 增氧氧浓度缩放 */
    uint8_t m_o2BoostTime;          /* 0x02 - 增氧运行时间 */
    uint8_t m_o2BoostTime_scale;    /* 增氧运行时间缩放 */
    uint8_t m_suctionTime;          /* 0x03 - 吸痰运行时间 */
    uint8_t m_suctionTime_scale;    /* 吸痰运行时间缩放 */
    uint8_t m_o2SourceType;         /* 0x04 - 氧气气源类型 */
    uint8_t m_o2SourceType_scale;   /* 氧气气源类型缩放 */
    uint8_t m_altitude;             /* 0x05 - 海拔高度 */
    uint8_t m_altitude_scale;       /* 海拔高度缩放 */
    uint8_t m_gasVolumeStd;         /* 0x06 - 气体体积校正标准 */
    uint8_t m_gasVolumeStd_scale;   /* 气体体积校正标准缩放 */
    uint8_t m_o2SensorSwitch;       /* 0x07 - 氧传感器开关 */
    uint8_t m_o2SensorSwitch_scale; /* 氧传感器开关缩放 */
    uint8_t m_ApneaVentType;        /* 0x08 - 窒息通气类型 */
    uint8_t m_ApneaVentType_scale;
    uint8_t m_InspType;             /* 0x09 - 吸气类型 */
    uint8_t m_InspType_scale;
    uint8_t m_LeakageComp;          /* 0x0A - 泄露补偿 */
    uint8_t m_LeakageComp_scale;    /* 泄露补偿缩放 */
    bool m_valid[PROTOCOL_SYSTEM_MENU_SUBID_MAX];                /* 有效性标志 */
} RxSystemMenuCache_t;

/* Manufacturer config cache (0xA7) */
#define PROTOCOL_MANUFACTURER_SUBID_MAX    2
typedef struct {
    uint8_t m_gasCorrectMode;       /* 0x00 - gas correction mode: 0 BTPS, 1 ATP */
    uint8_t m_gasCorrectMode_scale;
    uint16_t m_altitude;            /* 0x01 - altitude, m */
    uint8_t m_altitude_scale;
    bool m_valid[PROTOCOL_MANUFACTURER_SUBID_MAX];
} RxManufacturerCache_t;

/* 报警限缓存 (0xAC) */
#define PROTOCOL_ALARM_LIMITS_SUBID_MAX   11      /* 报警限子ID最大数量 */
typedef struct {
    uint16_t m_pAirwayLow;          /* 0x00 - 气道压低限 */
    uint8_t m_pAirwayLow_scale;     /* 气道压低限缩放 */
    uint16_t m_pAirwayHigh;         /* 0x01 - 气道压高限 */
    uint8_t m_pAirwayHigh_scale;    /* 气道压高限缩放 */
    uint16_t m_mvHigh;              /* 0x02 - 分钟通气量高限 */
    uint8_t m_mvHigh_scale;         /* 分钟通气量高限缩放 */
    uint16_t m_mvLow;               /* 0x03 - 分钟通气量低限 */
    uint8_t m_mvLow_scale;          /* 分钟通气量低限缩放 */
    uint16_t m_tveHigh;             /* 0x04 - 呼出潮气量高限 */
    uint8_t m_tveHigh_scale;        /* 呼出潮气量高限缩放 */
    uint16_t m_tveLow;              /* 0x05 - 呼出潮气量低限 */
    uint8_t m_tveLow_scale;         /* 呼出潮气量低限缩放 */
    uint8_t m_fio2High;             /* 0x06 - 氧浓度高限 */
    uint8_t m_fio2High_scale;       /* 氧浓度高限缩放 */
    uint8_t m_fio2Low;              /* 0x07 - 氧浓度低限 */
    uint8_t m_fio2Low_scale;        /* 氧浓度低限缩放 */
    uint8_t m_frTotalHigh;          /* 0x08 - 总频率高限 */
    uint8_t m_frTotalHigh_scale;    /* 总频率高限缩放 */
    uint8_t m_frTotalLow;           /* 0x09 - 总频率低限 */
    uint8_t m_frTotalLow_scale;     /* 总频率低限缩放 */
    uint8_t m_apneaTime;            /* 0x0A - 窒息时间 */
    uint8_t m_apneaTime_scale;      /* 窒息时间缩放 */
    bool m_valid[PROTOCOL_ALARM_LIMITS_SUBID_MAX];               /* 有效性标志 */
} RxAlarmLimitsCache_t;

/* 自检缓存 (0xAB) */
#define PROTOCOL_SELF_TEST_SUBID_MAX      12      /* 自检子ID最大数量 */
typedef struct {
    uint8_t m_system;               /* 0x00 - 系统自检 */
    uint8_t m_system_scale;         /* 系统自检缩放 */
    uint8_t m_turbine;              /* 0x01 - 涡轮测试 */
    uint8_t m_turbine_scale;        /* 涡轮测试缩放 */
    uint8_t m_o2FlowSensor;         /* 0x02 - 氧气流量传感器测试 */
    uint8_t m_o2FlowSensor_scale;   /* 氧气流量传感器测试缩放 */
    uint8_t m_inspFlowSensor;       /* 0x03 - 吸气流量传感器测试 */
    uint8_t m_inspFlowSensor_scale; /* 吸气流量传感器测试缩放 */
    uint8_t m_pressureSensor;       /* 0x04 - 压力传感器测试 */
    uint8_t m_pressureSensor_scale; /* 压力传感器测试缩放 */
    uint8_t m_expValve;             /* 0x05 - 呼气阀测试 */
    uint8_t m_expValve_scale;       /* 呼气阀测试缩放 */
    uint8_t m_safetyValve;          /* 0x06 - 安全阀测试 */
    uint8_t m_safetyValve_scale;    /* 安全阀测试缩放 */
    uint8_t m_leakTest;             /* 0x07 - 泄露量测试 */
    uint8_t m_leakTest_scale;       /* 泄露量测试缩放 */
    uint8_t m_complianceTest;       /* 0x08 - 顺应性测试 */
    uint8_t m_complianceTest_scale; /* 顺应性测试缩放 */
    uint8_t m_tubeResistanceTest;   /* 0x09 - 管路阻力测试 */
    uint8_t m_tubeResistanceTest_scale; /* 管路阻力测试缩放 */
    uint8_t m_proxFlowSensor;       /* 0x0A - 近端流量传感器测试 */
    uint8_t m_proxFlowSensor_scale; /* 近端流量传感器测试缩放 */
    uint8_t m_o2SensorTest;         /* 0x0B - 氧传感器测试 */
    uint8_t m_o2SensorTest_scale;   /* 氧传感器测试缩放 */
    bool m_valid[PROTOCOL_SELF_TEST_SUBID_MAX];               
} RxSelfTestCache_t;

/* 校准缓存 (0xAA) */
#define PROTOCOL_CALIBRATION_SUBID_MAX         8       /* 支持子ID 0x00~0x07 */
typedef struct {
    uint8_t m_zeroCalib;              /* 0x00 - 校零 */
    uint8_t m_zeroCalib_scale;        /* 校零缩放 */
    uint8_t m_pressureCalib;          /* 0x01 - 压力校准 */
    uint8_t m_pressureCalib_scale;    /* 压力校准缩放 */
    uint8_t m_proxFlowSensorCalib;    /* 0x02 - 近端流量传感器校准 */
    uint8_t m_proxFlowSensorCalib_scale; /* 近端流量传感器校准缩放 */
    uint8_t m_o2RatioValveCalib;      /* 0x03 - 氧气比例阀校准 */
    uint8_t m_o2RatioValveCalib_scale; /* 氧气比例阀校准缩放 */
    uint8_t m_airO2MixCoeffCalib;     /* 0x04 - 空氧混合系数校准 */
    uint8_t m_airO2MixCoeffCalib_scale; /* 空氧混合系数校准缩放 */
    uint8_t m_o2SensorCalib;          /* 0x05 - 氧气传感器校准 */
    uint8_t m_o2SensorCalib_scale;    /* 氧气传感器校准缩放 */
    uint8_t m_expValveCalib;          /* 0x06 - 呼气阀校准 */
    uint8_t m_expValveCalib_scale;    /* 呼气阀校准缩放 */
    uint8_t m_flowCalibPipe;           /* 0x07 - 流量校准管路：0x01 成人，0x02 婴幼儿 */
    uint8_t m_flowCalibPipe_scale;     /* 流量校准管路缩放 */
    bool m_valid[PROTOCOL_CALIBRATION_SUBID_MAX];                  /* 有效性标志 */
} RxCalibrationCache_t;

/**
 * 接收诊断缓存结构体
 * 对应图片中的16个参数，每个参数包含原始值和缩放值
 */
#define PROTOCOL_DIAGNOSIS_SUBID_MAX      16      /* 诊断子ID最大数量 */
typedef struct {
    // 小ID 0x00-0x0F 对应的16个参数，每个参数包含原始值和缩放值
    
    /* 0x00 - 诊断参数开关 (长度1, 范围0~1, 单位--) */
    uint8_t  m_diagnosisSwitch;         /* 原始值 */
    uint8_t  m_diagnosisSwitch_scale;   /* 缩放值 */
    
    /* 0x01 - 吸气压力校零阀控制 (长度1, 范围0~1, 单位--) */
    uint8_t  m_inspZeroValveCtrl;       /* 原始值 */
    uint8_t  m_inspZeroValveCtrl_scale; /* 缩放值 */
    
    /* 0x02 - 近端压力校零阀控制 (长度1, 范围0~1, 单位--) */
    uint8_t  m_proxPressureZero;        /* 原始值 */
    uint8_t  m_proxPressureZero_scale;  /* 缩放值 */
    
    /* 0x03 - 近端流速校零阀控制 (长度1, 范围0~1, 单位--) */
    uint8_t  m_proxFlowZero;            /* 原始值 */
    uint8_t  m_proxFlowZero_scale;      /* 缩放值 */
    
    /* 0x04 - 冲洗阀控制 (长度1, 范围0~1, 单位--) */
    uint8_t  m_flushValveCtrl;          /* 原始值 */
    uint8_t  m_flushValveCtrl_scale;    /* 缩放值 */
    
    /* 0x05 - 安全阀控制 (长度1, 范围0~100, 单位--) */
    uint8_t  m_safetyValveCtrl;         /* 原始值 */
    uint8_t  m_safetyValveCtrl_scale;   /* 缩放值 */
    
    /* 0x06 - 涡轮压力控制 (长度1, 范围0~100, 单位:cmH2O) */
    uint8_t  m_turbinePressureCtrl;     /* 原始值 */
    uint8_t  m_turbinePressureCtrl_scale; /* 缩放值 */
    
    /* 0x07 - 涡轮控制转速 (长度2, 范围0~65000, 单位:rpm) */
    uint16_t m_turbineSpeedCtrl;        /* 原始值 */
    uint16_t m_turbineSpeedCtrl_scale;  /* 缩放值 */
    
    /* 0x08 - 涡轮控制流量 (长度2, 范围0~300, 单位:L/min) */
    uint16_t m_turbineFlowCtrl;         /* 原始值 */
    uint16_t m_turbineFlowCtrl_scale;   /* 缩放值 */
    
    /* 0x09 - 涡轮控制占空比 (长度1, 范围0~100, 单位:%) */
    uint8_t  m_turbineDutyCycle;        /* 原始值 */
    uint8_t  m_turbineDutyCycle_scale;  /* 缩放值 */
    
    /* 0x0A - 氧气比例阀流速控制 (长度1, 范围0~200, 单位:L/min) */
    uint8_t  m_o2ValveFlowCtrl;         /* 原始值 */
    uint8_t  m_o2ValveFlowCtrl_scale;   /* 缩放值 */
    
    /* 0x0B - 氧气比例阀电流 (长度2, 范围0~1000, 单位:mA) */
    uint16_t m_o2ValveCurrent;          /* 原始值 */
    uint16_t m_o2ValveCurrent_scale;    /* 缩放值 */
    
    /* 0x0C - 氧气比例阀占空比 (长度1, 范围0~100, 单位:%) */
    uint8_t  m_o2ValveDutyCycle;        /* 原始值 */
    uint8_t  m_o2ValveDutyCycle_scale;  /* 缩放值 */
    
    /* 0x0D - 呼气比例阀压力控制 (长度1, 范围0~100, 单位:cmH2O) */
    uint8_t  m_exhValvePressureCtrl;    /* 原始值 */
    uint8_t  m_exhValvePressureCtrl_scale; /* 缩放值 */
    
    /* 0x0E - 呼气比例阀电流 (长度2, 范围0~2000, 单位:mA) */
    uint16_t m_exhValveCurrent;         /* 原始值 */
    uint16_t m_exhValveCurrent_scale;   /* 缩放值 */
    
    /* 0x0F - 呼气比例阀占空比 (长度1, 范围0~100, 单位:%) */
    uint8_t  m_exhValveDutyCycle;       /* 原始值 */
    uint8_t  m_exhValveDutyCycle_scale; /* 缩放值 */
    
    // 有效性标志数组，对应16个参数的原始值
    bool m_valid[PROTOCOL_DIAGNOSIS_SUBID_MAX];                   /* 16个参数的有效性标志 */
} RxDiagnosisCache_t;

/* 数据查询缓存 (0xA8) */
#define PROTOCOL_DATA_QUERY_SUBID_MAX     16      /* 数据查询子ID最大数量 */
typedef struct {
    uint8_t m_queryVersion;         /* 0x00 - 版本查询 */
    uint8_t m_queryVersion_scale;   /* 版本查询缩放 */
    uint8_t m_queryBootSelf;        /* 0x01 - 启动自检查询 */
    uint8_t m_queryBootSelf_scale;  /* 启动自检查询缩放 */
    uint8_t m_querySystemSelf;      /* 0x02 - 系统自检查询 */
    uint8_t m_querySystemSelf_scale;/* 系统自检查询缩放 */
    uint8_t m_queryMonitor;         /* 0x03 - 监测查询 */
    uint8_t m_queryMonitor_scale;   /* 监测查询缩放 */
    uint8_t m_queryDiagnosis;       /* 0x04 - 诊断查询 */
    uint8_t m_queryDiagnosis_scale; /* 诊断查询缩放 */
    uint8_t m_queryUserZero;        /* 0x05 - 用户零位查询 */
    uint8_t m_queryUserZero_scale;  /* 用户零位查询缩放 */
    uint8_t m_queryFactoryZero;     /* 0x06 - 工厂零位查询 */
    uint8_t m_queryFactoryZero_scale; /* 工厂零位查询缩放 */
    uint8_t m_queryPressCalib;      /* 0x07 - 压力校准查询 */
    uint8_t m_queryPressCalib_scale; /* 压力校准查询缩放 */
    uint8_t m_queryUserProx;        /* 0x08 - 用户近端查询 */
    uint8_t m_queryUserProx_scale;  /* 用户近端查询缩放 */
    uint8_t m_queryFactoryProx;     /* 0x09 - 工厂近端查询 */
    uint8_t m_queryFactoryProx_scale; /* 工厂近端查询缩放 */
    uint8_t m_queryUserO2;          /* 0x0A - 用户氧浓度查询 */
    uint8_t m_queryUserO2_scale;    /* 用户氧浓度查询缩放 */
    uint8_t m_queryFactoryO2;       /* 0x0B - 工厂氧浓度查询 */
    uint8_t m_queryFactoryO2_scale; /* 工厂氧浓度查询缩放 */
    uint8_t m_queryO2Valve;         /* 0x0C - 氧阀查询 */
    uint8_t m_queryO2Valve_scale;   /* 氧阀查询缩放 */
    uint8_t m_queryAirO2Coeff;      /* 0x0D - 空气氧系数查询 */
    uint8_t m_queryAirO2Coeff_scale; /* 空气氧系数查询缩放 */
    uint8_t m_queryExpValve;        /* 0x0E - 呼气阀查询 */
    uint8_t m_queryExpValve_scale;  /* 呼气阀查询缩放 */
    uint8_t m_queryO2SrcCalib;      /* 0x0F - 氧源校准查询 */
    uint8_t m_queryO2SrcCalib_scale; /* 氧源校准查询缩放 */
    bool m_valid[PROTOCOL_DATA_QUERY_SUBID_MAX];               /* 有效性标志 */
} RxDataQueryCache_t;

/* 在线升级缓存 (0xA6) */
#define PROTOCOL_ONLINE_UPGRADE_SUBID_MAX  1       /* 在线升级子ID最大数量 */
typedef struct {
    uint8_t m_upgradeCommand;       /* 0x00 - 升级命令 */
    uint8_t m_upgradeCommand_scale; /* 升级命令缩放 */
    bool m_valid[PROTOCOL_ONLINE_UPGRADE_SUBID_MAX];               /* 有效性标志 */
} RxOnlineUpgradeCache_t;

/* ==================== 发送主ID缓存结构体定义 ==================== */

/* 波形数据缓存 (0xAF) */
typedef struct {
    uint8_t m_breathPhase;          /* 呼吸相位 */
    uint16_t m_pressure;            /* 压力值 */
    uint16_t m_flow;                /* 流量值 */
    uint16_t m_volume;              /* 容积值 */
    bool m_valid;                   /* 有效性 */
} TxWaveDataCache_t;

/* 监测参数缓存 (0xAE) */
#define PROTOCOL_MONITOR_PARAMS_SUBID_MAX    49      /* 监测参数子ID最大数量 */
typedef struct {
    uint8_t m_fio2;                 /* 0x00 - 氧浓度 */
    int16_t m_pPeak;                /* 0x01 - 峰值压 */
    int16_t m_pPlat;                /* 0x02 - 平台压 */
    int16_t m_pMean;                /* 0x03 - 平均压 */
    uint16_t m_peep;                 /* 0x04 - 呼末正压 */
    uint16_t m_tvi;                  /* 0x05 - 吸入潮气量 */
    uint16_t m_tve;                  /* 0x06 - 呼出潮气量 */
    uint16_t m_tveSpn;               /* 0x07 - 自主呼出潮气量 */
    uint16_t m_mvi;                  /* 0x08 - 吸入分钟通气量 */
    uint16_t m_mve;                  /* 0x09 - 呼出分钟通气量 */
    uint16_t m_mvSpn;                /* 0x0A - 自主分钟通气量 */
    uint16_t m_mvLeak;               /* 0x0B - 分钟泄漏量 */
    uint16_t m_leakPercent;          /* 0x0C - 泄露百分比 */
    uint16_t m_flow;                 /* 0x0D - 流量 */
    uint16_t m_inspFlow;             /* 0x0E - 吸气峰值流量 */
    uint16_t m_expFlow;              /* 0x0F - 呼气峰值流量 */
    uint16_t m_frTotal;              /* 0x10 - 总频率 */
    uint16_t m_frMand;               /* 0x11 - 机控呼吸频率 */
    uint16_t m_frSpn;                /* 0x12 - 自主呼吸机频率 */
    uint16_t m_ti;                   /* 0x13 - 吸气时间 */
    uint16_t m_te;                   /* 0x14 - 呼气时间 */
    float m_ie;                      /* 0x15 - 吸呼比 */
    uint16_t m_rInsp;                /* 0x16 - 吸气气阻 */
    uint16_t m_rExp;                 /* 0x17 - 呼气气阻 */
    uint16_t m_cStat;                /* 0x18 - 静态顺应性 */
    uint16_t m_cDyn;                 /* 0x19 - 动态顺应性 */
    uint16_t m_rcexp;                /* 0x1A - 呼气时间常数 */
    uint16_t m_wob;                  /* 0x1B - 呼吸功 */
    uint16_t m_peepi;                /* 0x1C - 内源性呼末正压 */
    uint16_t m_peepTotal;            /* 0x1D - 总呼末正压 */
    int16_t m_p01;                   /* 0x1E - 口腔闭合压 */
    int16_t m_nif;                   /* 0x1F - 最大吸气负压 */
    uint16_t m_ptp;                  /* 0x20 - 压力时间乘积 */
    uint16_t m_tveIbw;               /* 0x21 - 理想体重潮气量比 */
    uint16_t m_o2SrcPress;           /* 0x22 - 氧气气源压力 */
    uint16_t m_Reserved1;            /* 0x23 - 保留 */
    uint16_t m_Reserved2;            /* 0x24 - 保留 */
    uint16_t m_stretchIndex;         /* 0x25 - 牵张指数 (范围: 0.5~1.5) */
    uint16_t m_lungOverinflation;    /* 0x26 - 肺过度膨胀系数 (范围: 0.00~5.00) */
    uint16_t m_brCo2Output;          /* 0x27 - 单次呼吸二氧化碳排放量 (范围: 0~200) */
    uint16_t m_spO2FiO2Ratio;        /* 0x28 - 血氧饱和度/吸入氧浓度 (范围: 65~500) */
    uint16_t m_oxygenIndex;          /* 0x29 - 氧饱和度指数 (范围: 0~30.0 cmH2O) */
    uint16_t m_meanPressO2Product;   /* 0x2A - 平均压与氧浓度乘积 (范围: 0~25.00 cmH2O) */
    uint16_t m_roxIndex;             /* 0x2B - ROX指数 (范围: 0~500.0) */
    uint16_t m_mechEnergy;           /* 0x2C - 机械能 (范围: 0~100.0 J/min) */
    uint16_t m_drivingPressure;      /* 0x2D - 驱动压 (范围: 0~120 cmH2O) */
    uint16_t m_O2TherapyFlow;        /* 0x2E - 氧疗流速(范围: 2~80 L/min) */
    uint16_t m_Reserved3;            /* 0x2F - 陷气体保留，需要合并 */
    uint16_t m_RSBI;                 /* 0x30 - 浅快呼吸机指数 (范围: 0~9999) */
    bool m_valid[PROTOCOL_MONITOR_PARAMS_SUBID_MAX];                /* 有效性标志 */
} TxMonitorParamsCache_t;

/* 生理报警缓存 (0xAD) */
typedef struct {
    uint8_t m_alarmBytes[4];         /* 报警位图 */
    bool m_valid;                    /* 有效性 */
} TxPhysAlarmCache_t;

/* 技术报警缓存 (0xAB) */
typedef struct {
    uint8_t m_alarmBytes[4];         /* 报警位图 */
    bool m_valid;                    /* 有效性 */
} TxTechAlarmCache_t;

/* 呼吸机报警事件枚举定义*/
typedef enum {
    AIRWAY_PRESSURE_HIGH,          // 气道压力过高
    AIRWAY_PRESSURE_LOW,           // 气道压力过低
    FIO2_HIGH,                     // FiO2过高
    FIO2_LOW,                      // FiO2过低
    EXPIRATORY_TIDAL_VOLUME_HIGH,  // 呼出潮气量过高
    EXPIRATORY_TIDAL_VOLUME_LOW,   // 呼出潮气量过低
    EXPIRATORY_MINUTE_VENTILATION_HIGH,  // 呼出分钟通气量过高
    EXPIRATORY_MINUTE_VENTILATION_LOW,   // 呼出分钟通气量过低
    APNEA_ALARM,                   // 窒息报警
    APNEA_VENTILATION_ALARM,     // 窒息通气报警
    APNEA_VENTILATION_END,       // 窒息通气结束
    RESPIRATORY_RATE_HIGH,         // 呼吸频率过高
    RESPIRATORY_RATE_LOW,          // 呼吸频率过低
    PHYSALARM_RESERVE1,          // 保留
    PHYSALARM_RESERVE2,          // 保留
    INVERSE_VENTILATION_ALARM,   // 反比通气报警

    VENTILATOR_EVENT_MAX
} VentilatorEvent;

/* 特殊功能状态缓存 (0xA9) */
#define PROTOCOL_SPECIAL_FUNC_SUBID_MAX      14      /* 特殊功能子ID最大数量 */
typedef struct {
    uint8_t m_manual;                /* 0x00 - 手动 */
    uint8_t m_inspHold;              /* 0x01 - 吸气保持 */
    uint8_t m_exhHold;               /* 0x02 - 呼气保持 */
    uint8_t m_o2Boost;               /* 0x03 - 增氧 */
    uint8_t m_si;                    /* 0x04 - SI */
    uint8_t m_sbt;                   /* 0x05 - SBT */
    uint8_t m_o2Therapy;             /* 0x06 - 氧疗 */
    uint8_t m_cprv;                  /* 0x07 - CPRV */
    uint8_t m_o2ConsTool;            /* 0x08 - 氧浓度工具 */
    uint8_t m_P01;                   /* 0x09 - P0.1 */
    uint8_t m_nif;                   /* 0x0A - NIF */
    uint8_t m_peepi;                 /* 0x0B - PEEPi */
    uint8_t m_pv;                    /* 0x0C - P-V */
    uint8_t m_cprVent;               /* 0x0D - CPR ventilation */
    bool m_valid[PROTOCOL_SPECIAL_FUNC_SUBID_MAX];                 /* 有效性标志 */
} TxSpecialFuncCache_t;

/* 校准数据缓存 (0xA8) */
#define PROTOCOL_CALIB_DATA_SUBID_MAX        22      /* 校准数据子ID最大数量 */
typedef struct {
    uint8_t m_sendProgress;             /* 0x00 - 发送进度条信息 */
    uint8_t  m_currentCalibItem;        /* 0x01 - 当前校准项目 */
    uint8_t  m_calibrationResult;       /* 0x02 - 校准结果 */
    uint8_t  m_faultCode;               /* 0x03 - 故障代码 */
    uint16_t m_inspPressureAdZeroHist;  /* 0x04 - 吸气压力AD零点历史 */
    uint16_t m_expPressureAdZeroHist;   /* 0x05 - 呼气压力AD零点历史 */
    uint16_t m_peepPressureAdZeroHist;  /* 0x06 - PEEP压力AD零点历史 */
    uint16_t m_proxFlowAdZeroHist;      /* 0x07 - 近端流量AD零点历史 */
    uint16_t m_inspPressureAd;          /* 0x08 - 吸气压力AD */
    uint16_t m_expPressureAd;           /* 0x09 - 呼气压力AD */
    uint16_t m_peepPressureAd;          /* 0x0A - PEEP压力AD */
    uint16_t m_proxFlowAd;              /* 0x0B - 近端流量AD */
    uint8_t  m_TestIndex;               /* 0x0C - 测试索引 */
    uint16_t m_MotorSpeed;              /* 0x0D - 电机转速 */
    uint16_t m_PressureValue;           /* 0x0E - 压力值 */
    int16_t m_FlowValue;                /* 0x0F - 近端流量值 */
    uint16_t m_O2ValveAd;               /* 0x10 - 氧气比例阀AD */
    int16_t m_O2ValveFlow;              /* 0x11 - 氧气比例阀流量 */
    uint16_t m_MotorDutyCycle;          /* 0x12 - 电机占空比 */
    int16_t m_TotalFlow;                /* 0x13 - 总支路流量 */
    int16_t m_O2Flow;                   /* 0x14 - 氧气流量 */
    uint16_t m_AirO2MixCoeff;           /* 0x15 - 空氧混合系数 */
    bool m_valid[PROTOCOL_CALIB_DATA_SUBID_MAX];                   /* 有效性标志 */
} TxCalibDataCache_t;

/* 诊断数据缓存 (0xA7) */
#define PROTOCOL_DIAG_DATA_SUBID_MAX         45      /* 诊断数据子ID最大数量 */
typedef struct {
    uint16_t pInspAd;         // 0 - 吸气压力(AD)，长度2
    int16_t pInsp;           // 1 - 吸气压力，长度2
    uint16_t pPeepAd;         // 2 - PEEP压力(AD)，长度2
    int16_t pPeep;           // 3 - PEEP压力，长度2
    uint16_t pExpAd;          // 4 - 呼气压力(AD)，长度2
    int16_t pExp;            // 5 - 呼气压力，长度2
    int16_t qInsp;           // 6 - 吸气流量，长度2
    uint16_t qProxAd;         // 7 - 近端流量(AD)，长度2
    int16_t qProx;           // 8 - 近端流量，长度2
    int16_t qO2;             // 9 - 氧气流量，长度2
    int8_t  tempInsp;        // 10 - 吸气支路气体温度，长度1
    int8_t  tempO2;          // 11 - 氧气支路气体温度，长度1
    uint8_t  o2Conc;          // 12 - 氧浓度，长度1
    int8_t  tempO2Sensor;    // 13 - 氧传感器温度，长度1
    uint8_t  humidO2Sensor;   // 14 - 氧传感器湿度，长度1
    uint16_t pressO2Sensor;   // 15 - 氧传感器大气压，长度2
    uint16_t flowO2Sensor;    // 16 - 氧传感器流量，长度2
    uint16_t pAtmAd;          // 17 - 大气压(AD)，长度2
    uint16_t pAtm;            // 18 - 大气压，长度2
    uint16_t tempAmbientAd;   // 19 - 大气温度(AD)，长度2
    uint8_t  tempAmbient;     // 20 - 大气温度，长度1
    uint16_t pressNeg;        // 21 - 负压传感器压力，长度2
    int8_t  tempNeg;         // 22 - 负压传感器温度，长度1
    int8_t  tempTurbine;     // 23 - 涡轮外部温度，长度1
    uint16_t hwVerVcmAd;      // 24 - VCM硬件版本(AD)，长度2
    uint32_t hwVerVcm;        // 25 - VCM硬件版本，长度4
    uint16_t oxygenControlValveCurrentAd;      // 26: 氧气控制阀电流(AD)，长度2
    uint16_t oxygenControlValveCurrent;        // 27: 氧气控制阀电流，长度2
    uint16_t exhalationControlValveCurrentAd;  // 28: 呼气控制阀电流(AD)，长度2
    uint16_t exhalationControlValveCurrent;    // 29: 呼气控制阀电流，长度2
    uint16_t safetyValveCurrentAd;            // 30: 安全阀电流(AD)，长度2
    uint16_t safetyValveCurrent;               // 31: 安全阀电流，长度2
    uint8_t inhalationPressureZeroValveStatus;  // 32: 吸气压力校零阀状态，长度1
    uint8_t proximalPressureZeroValveStatus;    // 33: 近端压力校零阀状态，长度1
    uint8_t proximalFlowZeroValveStatus;        // 34: 近端流量校零阀状态，长度1
    uint8_t flushValveStatus;                   // 35: 冲洗阀状态，长度1
    uint16_t avdd5vAD;           // 36: AVDD5V AD值，长度2
    uint16_t avdd5v;             // 37: AVDD5V，长度2
    uint16_t pcmVdd3v3AD;        // 38: PCM VDD3V3 AD值，长度2
    uint16_t pcmVdd3v3;          // 39: PCM VDD3V3，长度2
    uint16_t vdd26vAD;           // 40: VDD26V AD值，长度2
    uint16_t vdd26v;             // 41: VDD26V，长度2
    uint16_t turbo24vVoltageAD;  // 42: 涡轮24V电压 AD值，长度2
    uint16_t turbo24vVoltage;    // 43: 涡轮24V电压，长度2
    uint16_t turbineSpeed;       // 44: 涡轮转速，长度2
    
    bool m_valid[PROTOCOL_DIAG_DATA_SUBID_MAX];                 /* 有效性标志 */
} TxDiagDataCache_t;


/* 结果消息缓存 (0xA6) */
#define PROTOCOL_RESULT_MSG_SUBID_MAX       17      /* 结果消息子ID最大数量 */
typedef struct {
    uint8_t m_progress;               /* 0x00 - 发送进度条信息 */
    uint64_t m_bootSelfResult;        /* 0x01 - 开机自检结果*/
    uint8_t m_reserved02;             /* 0x02 - 保留 */
    uint8_t m_turbineTest;            /* 0x03 - 涡轮测试 */
    uint8_t m_o2FlowSensorTest;       /* 0x04 - 氧气流量传感器测试 */
    uint8_t m_inspFlowSensorTest;     /* 0x05 - 吸气流量传感器测试 */
    uint8_t m_pressureSensorTest;     /* 0x06 - 压力传感器测试 */
    uint8_t m_expValveTest;           /* 0x07 - 呼气阀测试 */
    uint8_t m_safetyValveTest;        /* 0x08 - 安全阀测试 */
    uint8_t m_leakTest;               /* 0x09 - 泄露量测试 */
    uint8_t m_complianceTest;         /* 0x0A - 顺应性测试 */
    uint8_t m_pipeResistanceTest;     /* 0x0B - 管路阻力测试 */
    uint8_t m_proxFlowSensorTest;     /* 0x0C - 近端流量传感器测试 */
    uint8_t m_o2SensorTest;           /* 0x0D - 氧传感器测试 */
    uint16_t m_pipeLeakage;            /* 0x0E - 管路泄漏量值 */
    uint16_t m_pipeCompliance;         /* 0x0F - 顺应性值 */
    uint16_t m_pipeResistance;         /* 0x10 - 管路阻力值 */
    bool m_valid[PROTOCOL_RESULT_MSG_SUBID_MAX];                 /* 有效性标志 */
} TxResultMsgCache_t;

/* 错误请求缓存 (0xA5) */
#define PROTOCOL_ERROR_REQUEST_SUBID_MAX     2       /* 错误请求子ID最大数量 */
typedef struct {
    uint8_t m_resendParams;           /* 0x00 - 重发参数 */
    uint8_t m_enterSafeMode;          /* 0x01 - 进入安全模式 */
    bool m_valid[PROTOCOL_ERROR_REQUEST_SUBID_MAX];                  /* 有效性标志 */
} TxErrorRequestCache_t;


/* 版本信息 （0xA4）*/
#define PROTOCOL_VERSION_INFO_SUBID_MAX     2       /* 版本信息子ID最大数量 */
typedef struct {
    uint32_t m_swVersion;              /* 0x00 - 软件版本 */
    uint16_t m_hwVersion;              /* 0x01 - 硬件版本 */ 
    bool m_valid[PROTOCOL_VERSION_INFO_SUBID_MAX];                  /* 有效性标志 */
} TxVersionInfoCache_t;
/* ==================== 监测参数大小和缩放比例枚举定义 ==================== */
typedef enum {
    // 参数大小定义 (字节数)
    E_FIO2_SIZE = 1,
    E_PPEAK_SIZE = 2,
    E_PPLAT_SIZE = 2,
    E_PMEAN_SIZE = 2,
    E_PEEP_SIZE = 2,
    E_TVI_SIZE = 2,
    E_TVE_SIZE = 2,
    E_TVESPN_SIZE = 2,
    E_MVI_SIZE = 2,
    E_MVE_SIZE = 2,
    E_MVSPN_SIZE = 2,
    E_MVLEAK_SIZE = 2,
    E_LEAKPERCENT_SIZE = 1,
    E_FLOW_SIZE = 2,
    E_INSPFLOW_SIZE = 2,
    E_EXPFLOW_SIZE = 2,
    E_FRTOTAL_SIZE = 1,
    E_FRMAND_SIZE = 1,
    E_FRSPN_SIZE = 1,
    E_TI_SIZE = 2,
    E_TE_SIZE = 2,
    E_IE_SIZE = 4,
    E_RINSP_SIZE = 2,
    E_REXP_SIZE = 2,
    E_CSTAT_SIZE = 2,
    E_CDYN_SIZE = 2,
    E_RCEXP_SIZE = 2,
    E_WOB_SIZE = 2,
    E_PEEPI_SIZE = 2,
    E_PEEPTOTAL_SIZE = 2,
    E_P01_SIZE = 2,
    E_NIF_SIZE = 2,
    E_PTP_SIZE = 2,
    E_TVEIBW_SIZE = 2,
    E_O2SRCPRESS_SIZE = 2,
    E_INSP_PEAK_FLOW_SIZE = 1,     // 0x23 - 吸气峰值流速
    E_EXP_PEAK_FLOW_SIZE = 1,      // 0x24 - 呼气峰值流速
    E_STRETCH_INDEX_SIZE = 1,      // 0x25 - 牵张指数
    E_LUNG_OVERINFLATION_SIZE = 2, // 0x26 - 肺过度膨胀系数
    E_BR_CO2_OUTPUT_SIZE = 1,     // 0x27 - 单次呼吸二氧化碳排放量
    E_SPO2_FIO2_RATIO_SIZE = 2,   // 0x28 - 血氧饱和度/吸入氧浓度
    E_OXYGEN_INDEX_SIZE = 2,      // 0x29 - 氧饱和度指数
    E_MEAN_PRESS_O2_PRODUCT_SIZE = 2, // 0x2A - 平均压与氧浓度乘积
    E_ROX_INDEX_SIZE = 2,         // 0x2B - ROX指数
    E_MECH_ENERGY_SIZE = 2,       // 0x2C - 机械能
    E_DRIVING_PRESSURE_SIZE = 2,   // 0x2D - 驱动压
    E_O2_THERAPY_FLOW_SIZE = 2,    // 0x2E - 氧疗流速
    E_RSBI_SIZE = 2,               // 0x2F - 浅快呼吸指数
    
    // 缩放比例定义 (0=x1, 1=x10, 2=x100 3=x1000)
    E_FIO2_SCALE = 0,
    E_PPEAK_SCALE = 1,
    E_PPLAT_SCALE = 1,
    E_PMEAN_SCALE = 1,
    E_PEEP_SCALE = 1,
    E_TVI_SCALE = 0,
    E_TVE_SCALE = 0,
    E_TVESPN_SCALE = 0,
    E_MVI_SCALE = 1,
    E_MVE_SCALE = 1,
    E_MVSPN_SCALE = 1,
    E_MVLEAK_SCALE = 1,
    E_LEAKPERCENT_SCALE = 0,
    E_FLOW_SCALE = 1,
    E_INSPFLOW_SCALE = 1,
    E_EXPFLOW_SCALE = 1,
    E_FRTOTAL_SCALE = 0,
    E_FRMAND_SCALE = 0,
    E_FRSPN_SCALE = 0,
    E_TI_SCALE = 2,
    E_TE_SCALE = 2,
    E_IE_SCALE = 0,
    E_RINSP_SCALE = 0,
    E_REXP_SCALE = 0,
    E_CSTAT_SCALE = 1,
    E_CDYN_SCALE = 1,
    E_RCEXP_SCALE = 2,
    E_WOB_SCALE = 1,
    E_PEEPI_SCALE = 2,
    E_PEEPTOTAL_SCALE = 2,
    E_P01_SCALE = 2,
    E_NIF_SCALE = 2,
    E_PTP_SCALE = 2,
    E_TVEIBW_SCALE = 1,
    E_O2SRCPRESS_SCALE = 0,
    E_INSP_PEAK_FLOW_SCALE = 0,    // 0x23 - 吸气峰值流速 (范围: 0~210 L/min)
    E_EXP_PEAK_FLOW_SCALE = 0,     // 0x24 - 呼气峰值流速 (范围: 0~210 L/min)
    E_STRETCH_INDEX_SCALE = 2,   // 0x25 - 牵张指数 (范围: 0.5~1.5)
    E_LUNG_OVERINFLATION_SCALE = 2, // 0x26 - 肺过度膨胀系数 (范围: 0.00~5.00)
    E_BR_CO2_OUTPUT_SCALE = 0,     // 0x27 - 单次呼吸二氧化碳排放量 (范围: 0~200)
    E_SPO2_FIO2_RATIO_SCALE = 0,   // 0x28 - 血氧饱和度/吸入氧浓度 (范围: 65~500)
    E_OXYGEN_INDEX_SCALE = 2,    // 0x29 - 氧饱和度指数 (范围: 0~30.0 cmH2O)
    E_MEAN_PRESS_O2_PRODUCT_SCALE = 2, // 0x2A - 平均压与氧浓度乘积 (范围: 0~25.00 cmH2O)
    E_ROX_INDEX_SCALE = 2,       // 0x2B - ROX指数 (范围: 0~500.0)
    E_MECH_ENERGY_SCALE = 2,     // 0x2C - 机械能 (范围: 0~100.0 J/min)
    E_DRIVING_PRESSURE_SCALE = 0,   // 0x2D - 驱动压 (范围: 0~120 cmH2O)
    E_O2_THERAPY_FLOW_SCALE = 1,  // 0x2E - 氧疗流速 (范围: 2~80 L/min)
    E_RSBI_SCALE = 0              // 0x2F - 浅快呼吸指数 (范围: 0~9999)
} MonitorParamSizeScaleEnum;


typedef struct {
    bool isInProgress;
    uint8_t curTest;
    uint8_t status;
} SelfCheckData_t;

/* ==================== 波形数据结构 ==================== */

/* 波形数据结构 */
typedef struct {
    uint8_t m_breathPhase;          /* 呼吸相位 */
    int16_t m_pressure;             /* 压力值 */
    int16_t m_flow;                 /* 流量值 */
    uint16_t m_volume;              /* 容积值 */
    uint32_t m_timestamp;           /* 时间戳（可选） */
} WaveData_t;

/* 报警数据结构 */
typedef struct {
    uint8_t m_alarmBytes[4];        /* 报警位图 (4字节) */
} AlarmData_t;

/* ==================== 工作模式枚举定义 ==================== */
typedef enum {
    P_A_C_MODE = 0,         // P-A/C
    V_A_C_MODE,             // V-A/C
    P_SIMV_MODE,            // P-SIMV
    V_SIMV_MODE,            // V-SIMV
    CPAP_PSV_MODE,          // CPAP/PSV
    PRVC_MODE,              // PRVC
    PRVC_SIMV_MODE,         // PRVC-SIMV
    DAPAP_MODE,             // Dapap
    CPRV_MODE,              // CPRV
    APNEA_VENTILATION_MODE, // 窒息通气模式
    OXYGEN_THERAPY_MODE,    // 氧疗
    APRV_MODE,              // APRV
    VS_MODE,                // VS
    AMV_MODE,               // AMV
    NCPAP_MODE,             // nCPAP
    NCPAP_PC_MODE,           // nCPAP-PC
    MAX_VENTILATION_MODE
} VentilationMode;
/* ==================== 协议地址定义 ==================== */
/**
 * @brief 接收数据包处理函数
 * @param instance USART实例
 */
void ProtocolProcessRxPacket(const ProtocolPacket_t* packet);
/* ==================== 接收缓存更新函数 ==================== */
/**
 * @brief 更新接收的监测参数缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxVentParamsCache(const ProtocolPacket_t* packet);
/**
 * @brief 更新接收的波形数据缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxVentSwitchCache(const ProtocolPacket_t* packet);
/**
 * @brief 更新接收的系统菜单缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxSystemMenuCache(const ProtocolPacket_t* packet);

/**
 * @brief Update received manufacturer config cache
 * @param packet received packet
 */
void ProtocolUpdateRxManufacturerCache(const ProtocolPacket_t* packet);
/**
 * @brief 更新接收的报警限值缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxAlarmLimitsCache(const ProtocolPacket_t* packet);
/**
 * @brief 更新接收的自检缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxSelfTestCache(const ProtocolPacket_t* packet);
/**
 * @brief 更新接收的校准缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxCalibrationCache(const ProtocolPacket_t* packet);
/**
 * @brief 更新接收的诊断缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxDiagnosisCache(const ProtocolPacket_t* packet);
/**
 * @brief 更新接收的数据查询缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxDataQueryCache(const ProtocolPacket_t* packet);
/**
 * @brief 更新接收的在线升级缓存
 * @param packet 接收到的数据包
 */
void ProtocolUpdateRxOnlineUpgradeCache(const ProtocolPacket_t* packet);

/**
 * @brief 从缓存发送监测参数
 * @param instance USART实例
 */
void ProtocolSendMonitorParamsFromCache(uint8_t instance);
/* ==================== 发送数据处理函数 ==================== */
/**
 * @brief 预处理发送的协议数据
 * @param instance USART实例
 */
void ProtocolDataPreProcess(uint8_t instance);
/**
 * @brief 处理发送的监测参数数据
 * @param instance USART实例
 */
void ProtocolMonitorParamsProcess(uint8_t instance);
/**
 * @brief 处理发送的波形数据
 * @param instance USART实例
 * @param taskCounter 任务计数器
 */
void ProtocolWaveDataProcess(uint8_t instance, uint32_t taskCounter);
/**
 * @brief 处理发送的波形数据
 * @param instance USART实例
 * @param taskCounter 任务计数器
 */
void ProtocolHeartbeatDataProcess(uint8_t instance, uint32_t taskCounter);
/**
 * @brief 处理发送的自检数据
 * @param instance USART实例
 * @param taskCounter 任务计数器
 */
void ProtocolDetectDataPreProcess(uint8_t instance, uint32_t taskCounter);
/**
 * @brief 处理发送的报警数据
 * @param instance USART实例
 * @param taskCounter 任务计数器
 */
void ProtocolPhysAlarmDataProcess(uint8_t instance, uint32_t taskCounter);


bool ProtocolIsMCMConnected(void);

/* ==================== 协议缓存访问函数 ==================== */
/**
 * @brief 获取通气参数缓存指针
 * @return RxVentParamsCache_t* 通气参数缓存指针
 */
const RxVentParamsCache_t* ProtocolGetRxVentParamsCache(void);

/**
 * @brief 获取通气开关缓存指针
 * @return RxVentSwitchCache_t* 通气开关缓存指针
 */
const RxVentSwitchCache_t* ProtocolGetRxVentSwitchCache(void);

/**
 * @brief 获取系统菜单缓存指针
 * @return RxSystemMenuCache_t* 系统菜单缓存指针
 */
const RxSystemMenuCache_t* ProtocolGetRxSystemMenuCache(void);

/**
 * @brief Get manufacturer config cache pointer
 * @return RxManufacturerCache_t* manufacturer config cache pointer
 */
const RxManufacturerCache_t* ProtocolGetRxManufacturerCache(void);

/**
 * @brief 获取报警限缓存指针
 * @return RxAlarmLimitsCache_t* 报警限缓存指针
 */
const RxAlarmLimitsCache_t* ProtocolGetRxAlarmLimitsCache(void);

/**
 * @brief 获取接收自检缓存指针
 * @return RxAlarmLimitsCache_t* 接收自检缓存指针
 */
const RxSelfTestCache_t* ProtocolGetRxSelfTestCache(void);

/**
 * @brief 获取接收自检信息指针
 * @return RxAlarmLimitsCache_t* 接收自检信息指针
 */
const SelfCheckData_t* ProtocolGetSelfTestInfoCache(void);

/**
 * @brief 获取接收校准缓存指针
 * @return RxAlarmLimitsCache_t* 接收校准缓存指针
 */
const RxDiagnosisCache_t* ProtocolGetRxDiagnosisCache(void);

/**
 * @brief 获取接收数据查询缓存指针
 * @return RxAlarmLimitsCache_t* 接收数据查询缓存指针
 */
const RxCalibrationCache_t* ProtocolGetRxCalibDataCache(void);
#ifdef __cplusplus
}
#endif
#endif  // EXAMPLE_H
/**************************End of file********************************/
