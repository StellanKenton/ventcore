/************************************************************************************
* @file     : triggerengine.h
* @brief    : Breath trigger engine interface.
* @details  : Declares trigger engine initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_TRIGGERENGINE_H
#define USER_APP_VENTLOGIC_TRIGGERENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the trigger engine. */
void triggerEngineInit(void);

/** Run one trigger engine processing cycle. */
void triggerEngineProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_TRIGGERENGINE_H */
/**************************End of file********************************/
