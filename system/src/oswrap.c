/*=============================================================================================*//**
@file    oswrap.c

@author  Daniel Zorychta

@brief   Operating system wrapper

@note    Copyright (C) 2012 Daniel Zorychta <daniel.zorychta@gmail.com>

         This program is free software; you can redistribute it and/or modify
         it under the terms of the GNU General Public License as published by
         the  Free Software  Foundation;  either version 2 of the License, or
         any later version.

         This  program  is  distributed  in the hope that  it will be useful,
         but  WITHOUT  ANY  WARRANTY;  without  even  the implied warranty of
         MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the
         GNU General Public License for more details.

         You  should  have received a copy  of the GNU General Public License
         along  with  this  program;  if not,  write  to  the  Free  Software
         Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


*//*==============================================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
                                            Include files
==================================================================================================*/
#include "oswrap.h"
#include "taskmoni.h"


/*==================================================================================================
                                  Local symbolic constants/macros
==================================================================================================*/
#define Priority(prio)                    (prio + (configMAX_PRIORITIES / 2))


/*==================================================================================================
                                   Local types, enums definitions
==================================================================================================*/


/*==================================================================================================
                                      Local function prototypes
==================================================================================================*/


/*==================================================================================================
                                      Local object definitions
==================================================================================================*/


/*==================================================================================================
                                     Exported object definitions
==================================================================================================*/


/*==================================================================================================
                                        Function definitions
==================================================================================================*/

//================================================================================================//
/**
 * @brief
 */
//================================================================================================//
int_t TaskCreate(taskCode_t taskCode, const ch_t *name, u16_t stack, void *argv, i8_t priority, task_t *taskHdl)
{
      TaskSuspendAll();

      task_t task;

      int_t status = xTaskCreate(taskCode, (signed char *)name, stack, argv, Priority(priority), &task);

      if (taskHdl) {
            *taskHdl = task;
      }

      if (status == OS_OK) {
            moni_AddTask(task);
      }

      TaskResumeAll();

      return status;
}


//================================================================================================//
/**
 * @brief
 */
//================================================================================================//
void TaskDelete(task_t taskHdl)
{
      moni_DelTask(taskHdl);
      vTaskDelete(taskHdl);
}


#ifdef __cplusplus
}
#endif

/*==================================================================================================
                                            End of file
==================================================================================================*/
