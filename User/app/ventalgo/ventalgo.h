/************************************************************************************
* @file     : ventalgo.h
* @brief    : Ventilation algorithm manager interface.
* @details  : Declares initialization and periodic processing for all controllers.
* @author   :
* @date     : 2026-08-24
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTALGO_VENTALGO_H
#define USER_APP_VENTALGO_VENTALGO_H

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize all ventilation controllers. */
void ventAlgoInit(void);

/** Run one processing cycle for all ventilation controllers. */
void ventAlgoProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_VENTALGO_H */
/**************************End of file********************************/
