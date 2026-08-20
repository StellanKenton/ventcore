/************************************************************************************
* @file     : cycleengine.h
* @brief    : Breath cycle engine interface.
* @details  : Declares cycle engine initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_CYCLEENGINE_H
#define USER_APP_VENTLOGIC_CYCLEENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the cycle engine. */
void cycleEngineInit(void);

/** Run one cycle engine processing cycle. */
void cycleEngineProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_CYCLEENGINE_H */
/**************************End of file********************************/
