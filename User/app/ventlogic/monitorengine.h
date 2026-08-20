/************************************************************************************
* @file     : monitorengine.h
* @brief    : Breath monitor engine interface.
* @details  : Declares monitor engine initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_MONITORENGINE_H
#define USER_APP_VENTLOGIC_MONITORENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the monitor engine. */
void monitorEngineInit(void);

/** Run one monitor engine processing cycle. */
void monitorEngineProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_MONITORENGINE_H */
/**************************End of file********************************/
