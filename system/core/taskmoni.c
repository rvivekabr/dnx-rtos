/*=============================================================================================*//**
@file    appmoni.c

@author  Daniel Zorychta

@brief   This module is used to monitoring all applications

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
#include "taskmoni.h"
#include "dlist.h"
#include "cpuctl.h"
#include "oswrap.h"
#include <string.h>


/*==================================================================================================
                                  Local symbolic constants/macros
==================================================================================================*/
#define calloc(nmemb, msize)        memman_calloc(nmemb, msize)
#define malloc(size)                memman_malloc(size)
#define free(mem)                   memman_free(mem)

#define MEM_BLOCK_COUNT             4
#define MEM_ADR_IN_BLOCK            7

#define FLE_BLOCK_COUNT             3
#define FLE_OPN_IN_BLOCK            7

#define DIR_BLOCK_COUNT             1
#define DIR_OPN_IN_BLOCK            3

#define MTX_BLOCK_TIME              10

/*==================================================================================================
                                   Local types, enums definitions
==================================================================================================*/
/* task information */
struct taskData {
#if (APP_MONITOR_MEMORY_USAGE > 0)
      struct memBlock {
            bool_t full;

            struct memSlot {
                void *addr;
                u32_t size;
            } mslot[MEM_ADR_IN_BLOCK];

      } *mblock[MEM_BLOCK_COUNT];
#endif

#if (APP_MONITOR_FILE_USAGE > 0)
      struct fileBlock {
            bool_t full;

            struct fileSlot {
                  FILE_t *file;
            } fslot[FLE_OPN_IN_BLOCK];

      } *fblock[FLE_BLOCK_COUNT];

      struct dirBlock {
            bool_t full;

            struct dirSlot {
                  DIR_t *dir;
            } dslot[DIR_OPN_IN_BLOCK];
      } *dblock[DIR_BLOCK_COUNT];
#endif
};

/* main structure */
struct moni {
      list_t  *tasks;         /* list with task informations */
      mutex_t  mtx;           /* mutex */
};


/*==================================================================================================
                                      Local function prototypes
==================================================================================================*/
#if ((APP_MONITOR_MEMORY_USAGE > 0) || (APP_MONITOR_FILE_USAGE > 0) || (APP_MONITOR_CPU_LOAD > 0))
static stdRet_t moni_Init(void);
#endif


/*==================================================================================================
                                      Local object definitions
==================================================================================================*/
#if ((APP_MONITOR_MEMORY_USAGE > 0) || (APP_MONITOR_FILE_USAGE > 0) || (APP_MONITOR_CPU_LOAD > 0))
static struct moni *moni;
#endif


/*==================================================================================================
                                     Exported object definitions
==================================================================================================*/


/*==================================================================================================
                                        Function definitions
==================================================================================================*/

//================================================================================================//
/**
 * @brief Initialize module
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
#if ((APP_MONITOR_MEMORY_USAGE > 0) || (APP_MONITOR_FILE_USAGE > 0) || (APP_MONITOR_CPU_LOAD > 0))
static stdRet_t moni_Init(void)
{
      stdRet_t status = STD_RET_OK;

      if (moni == NULL) {
            moni = calloc(1, sizeof(struct moni));

            if (moni) {
                  moni->tasks = ListCreate();
                  moni->mtx   = CreateRecMutex();

                  if (!moni->tasks || !moni->mtx) {
                        if (moni->tasks)
                              ListDelete(moni->tasks);

                        if (moni->mtx)
                              DeleteRecMutex(moni->mtx);

                        free(moni);
                        moni = NULL;

                        status = STD_RET_ERROR;
                  } else {
                        cpuctl_InitTimeStatCnt();
                  }
            }
      }

      return status;
}
#endif


//================================================================================================//
/**
 * @brief Function adds task to monitoring
 *
 * @param pid     task ID
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
#if ((APP_MONITOR_MEMORY_USAGE > 0) || (APP_MONITOR_FILE_USAGE > 0) || (APP_MONITOR_CPU_LOAD > 0))
stdRet_t moni_AddTask(task_t taskHdl)
{
      stdRet_t status = STD_RET_ERROR;

      if (moni == NULL) {
            moni_Init();
      }

      if (moni) {
            while (TakeRecMutex(moni->mtx, MTX_BLOCK_TIME) != OS_OK);

            struct taskData *task = ListGetItemDataByID(moni->tasks, (u32_t)taskHdl);

            /* task does not exist */
            if (task == NULL) {
                  task = calloc(1, sizeof(struct taskData));

                  if (task) {
                        if (ListAddItem(moni->tasks, (u32_t)taskHdl, task) >= 0) {
                              status = STD_RET_OK;
                        } else {
                              free(task);
                        }
                  }
            }

            GiveRecMutex(moni->mtx);
      }

      return status;
}
#endif


//================================================================================================//
/**
 * @brief Function delete task monitoring
 *
 * @param pid     task ID
 */
//================================================================================================//
#if ((APP_MONITOR_MEMORY_USAGE > 0) || (APP_MONITOR_FILE_USAGE > 0) || (APP_MONITOR_CPU_LOAD > 0))
stdRet_t moni_DelTask(task_t taskHdl)
{
      stdRet_t status = STD_RET_ERROR;

      while (TakeRecMutex(moni->mtx, MTX_BLOCK_TIME) != OS_OK);

      struct taskData *taskInfo = ListGetItemDataByID(moni->tasks, (u32_t)taskHdl);

      if (taskInfo) {
            #if (APP_MONITOR_MEMORY_USAGE > 0)
            for (u32_t block = 0; block < MEM_BLOCK_COUNT; block++) {
                  if (taskInfo->mblock[block]) {

                        for (u32_t slot = 0; slot < MEM_ADR_IN_BLOCK; slot++) {

                              struct memSlot *mslot = &taskInfo->mblock[block]->mslot[slot];

                              if (mslot->addr) {
                                    free(mslot->addr);
                                    mslot->size = 0;
                              }
                        }

                        free(taskInfo->mblock[block]);
                        taskInfo->mblock[block] = NULL;
                  }
            }
            #endif

            #if (APP_MONITOR_FILE_USAGE > 0)
            for (u32_t block = 0; block < FLE_BLOCK_COUNT; block++) {
                  if (taskInfo->fblock[block]) {

                        for (u32_t slot = 0; slot < FLE_OPN_IN_BLOCK; slot++) {

                              struct fileSlot *fslot = &taskInfo->fblock[block]->fslot[slot];

                              if (fslot->file) {
                                    vfs_fclose(fslot->file);
                              }
                        }

                        free(taskInfo->fblock[block]);
                        taskInfo->fblock[block] = NULL;
                  }
            }

            for (u32_t block = 0; block < DIR_BLOCK_COUNT; block++) {
                  if (taskInfo->dblock[block]) {

                        for (u32_t slot = 0; slot < DIR_OPN_IN_BLOCK; slot++) {

                              struct dirSlot *dslot = &taskInfo->dblock[block]->dslot[slot];

                              if (dslot->dir) {
                                    vfs_closedir(dslot->dir);
                              }
                        }

                        free(taskInfo->fblock[block]);
                        taskInfo->fblock[block] = NULL;
                  }
            }
            #endif

            ListRmItemByID(moni->tasks, (u32_t)taskHdl);

            status = STD_RET_OK;
      }

      GiveRecMutex(moni->mtx);

      return status;
}
#endif


//================================================================================================//
/**
 * @brief Function gets task statistics
 *
 * @param  item   task item
 * @param *stat   task statistics
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
#if ((APP_MONITOR_MEMORY_USAGE > 0) || (APP_MONITOR_FILE_USAGE > 0) || (APP_MONITOR_CPU_LOAD > 0))
stdRet_t moni_GetTaskStat(i32_t item, struct taskstat *stat)
{
      stdRet_t status = STD_RET_ERROR;

      if (moni && stat) {
            stat->memUsage  = 0;
            stat->openFiles = 0;

            while (TakeRecMutex(moni->mtx, MTX_BLOCK_TIME) != OS_OK);

            struct taskData *taskdata = ListGetItemDataByNo(moni->tasks, item);

            if (taskdata) {
                  #if (APP_MONITOR_MEMORY_USAGE > 0)
                  for (u32_t block = 0; block < MEM_BLOCK_COUNT; block++) {
                        if (taskdata->mblock[block]) {
                              for (u32_t slot = 0; slot < MEM_ADR_IN_BLOCK; slot++) {
                                    if (taskdata->mblock[block]->mslot[slot].addr) {
                                          stat->memUsage += taskdata->mblock[block]->mslot[slot].size;
                                    }
                              }
                        }
                  }
                  #endif

                  #if (APP_MONITOR_FILE_USAGE > 0)
                  for (u32_t block = 0; block < FLE_BLOCK_COUNT; block++) {
                        if (taskdata->fblock[block]) {
                              for (u32_t slot = 0; slot < FLE_OPN_IN_BLOCK; slot++) {
                                    if (taskdata->fblock[block]->fslot[slot].file) {
                                          stat->openFiles++;
                                    }
                              }
                        }
                  }

                  for (u32_t block = 0; block < DIR_BLOCK_COUNT; block++) {
                        if (taskdata->dblock[block]) {
                              for (u32_t slot = 0; slot < DIR_OPN_IN_BLOCK; slot++) {
                                    if (taskdata->dblock[block]->dslot[slot].dir) {
                                          stat->openFiles++;
                                    }
                              }
                        }
                  }
                  #endif

                  status = STD_RET_OK;
            }

            GiveRecMutex(moni->mtx);

            if (status == STD_RET_OK) {
                  task_t taskHdl = 0;
                  ListGetItemID(moni->tasks, item, (task_t)&taskHdl);

                  stat->handle        = taskHdl;
                  stat->name          = TaskGetName(taskHdl);
                  stat->freeStack     = TaskGetStackFreeSpace(taskHdl);
                  TaskSuspendAll();
                  stat->cpuUsage      = (u32_t)TaskGetTag(taskHdl);
                  TaskSetTag(taskHdl, (void*)0);
                  TaskResumeAll();
                  stat->cpuUsageTotal = cpuctl_GetCPUTotalTime();
                  stat->priority  = TaskGetPriority(taskHdl);

                  if (item == ListGetItemCount(moni->tasks) - 1) {
                        cpuctl_ClearCPUTotalTime();
                  }
            }
      }

      return status;
}
#endif


//================================================================================================//
/**
 * @brief
 */
//================================================================================================//
#if ((APP_MONITOR_MEMORY_USAGE > 0) || (APP_MONITOR_FILE_USAGE > 0) || (APP_MONITOR_CPU_LOAD > 0))
stdRet_t moni_GetTaskHdlStat(task_t taskHdl, struct taskstat *stat)
{
      stdRet_t status = STD_RET_ERROR;

      if (taskHdl) {
            i32_t item = -1;

            if (ListGetItemNo(moni->tasks, (u32_t)taskHdl, &item) == 0) {
                  status = moni_GetTaskStat(item, stat);
            }
      }

      return status;
}
#endif

//================================================================================================//
/**
 * @brief Function return number of monitor tasks
 *
 * @return number of monitor tasks
 */
//================================================================================================//
#if ((APP_MONITOR_MEMORY_USAGE > 0) || (APP_MONITOR_FILE_USAGE > 0) || (APP_MONITOR_CPU_LOAD > 0))
u16_t moni_GetTaskCount(void)
{
      return ListGetItemCount(moni->tasks);
}
#endif


//================================================================================================//
/**
 * @brief Monitor memory allocation
 *
 * @param size          block size
 *
 * @return pointer to allocated block or NULL if error
 */
//================================================================================================//
#if (APP_MONITOR_MEMORY_USAGE > 0)
void *moni_malloc(u32_t size)
{
      void *mem = NULL;

      if (moni) {
            while (TakeRecMutex(moni->mtx, MTX_BLOCK_TIME) != OS_OK);

            struct taskData *taskInfo = ListGetItemDataByID(moni->tasks, (u32_t)TaskGetCurrentTaskHandle());

            if (taskInfo) {
                  /* find empty address slot */
                  u32_t block = 0;

                  while (block < MEM_BLOCK_COUNT) {
                        if (taskInfo->mblock[block]) {

                              /* find empty address slot in block */
                              for (u8_t slot = 0; slot < MEM_ADR_IN_BLOCK; slot++) {
                                    if (taskInfo->mblock[block]->full == TRUE) {
                                          break;
                                    }

                                    struct memSlot *mslot = &taskInfo->mblock[block]->mslot[slot];

                                    if (mslot->addr == NULL) {
                                          mslot->addr = malloc(size);

                                          if (mslot->addr) {
                                                mem = mslot->addr;
                                                mslot->size = size;
                                          }

                                          if (slot == MEM_ADR_IN_BLOCK - 1)
                                                taskInfo->mblock[block]->full = TRUE;

                                          goto moni_malloc_end;
                                    }
                              }

                              block++;
                        } else {
                              /* create new block */
                              taskInfo->mblock[block] = calloc(1, sizeof(struct memBlock));

                              if (taskInfo->mblock[block] == NULL) {
                                    block++;
                              }
                        }
                  }
            }

            moni_malloc_end:
            GiveRecMutex(moni->mtx);
      }

      return mem;
}
#endif


//================================================================================================//
/**
 * @brief Monitor memory allocation
 *
 * @param nmemb         n members
 * @param msize         member size
 *
 * @return pointer to allocated block or NULL if error
 */
//================================================================================================//
#if (APP_MONITOR_MEMORY_USAGE > 0)
void *moni_calloc(u32_t nmemb, u32_t msize)
{
      void *ptr = moni_malloc(nmemb * msize);

      if (ptr)
            memset(ptr, 0, nmemb * msize);

      return ptr;
}
#endif


//================================================================================================//
/**
 * @brief Monitor memory freeing
 *
 * @param *mem          block to free
 */
//================================================================================================//
#if (APP_MONITOR_MEMORY_USAGE > 0)
void moni_free(void *mem)
{
      if (moni) {
            while (TakeRecMutex(moni->mtx, MTX_BLOCK_TIME) != OS_OK);

            struct taskData *taskInfo = ListGetItemDataByID(moni->tasks, (u32_t)TaskGetCurrentTaskHandle());

            if (taskInfo) {
                  /* find address slot */
                  for (u8_t block = 0; block < MEM_BLOCK_COUNT; block++) {

                        if (taskInfo->mblock[block]) {

                              /* find empty address slot in block */
                              for (u8_t slot = 0; slot < MEM_ADR_IN_BLOCK; slot++) {

                                    struct memSlot *mslot = &taskInfo->mblock[block]->mslot[slot];

                                    if (mslot->addr == mem) {
                                          free(mslot->addr);
                                          mslot->addr = NULL;
                                          mslot->size = 0;

                                          taskInfo->mblock[block]->full = FALSE;

                                          /* check if block is empty */
                                          if (block != 0) {
                                                for (u8_t i = 0; i < MEM_ADR_IN_BLOCK; i++) {
                                                      if (taskInfo->mblock[block]->mslot[i].addr != NULL)
                                                            goto moni_free_end;
                                                }

                                                /* block is empty - freeing memory */
                                                free(taskInfo->mblock[block]);
                                                taskInfo->mblock[block] = NULL;
                                          }

                                          goto moni_free_end;
                                    }
                              }
                        }
                  }

                  /* error: application try to free freed memory */
                  /* signal code can be added here */
            }

            moni_free_end:
            GiveRecMutex(moni->mtx);
      }
}
#endif


//================================================================================================//
/**
 * @brief Function open selected file
 *
 * @param *name         file path
 * @param *mode         file mode
 *
 * @retval NULL if file can't be created
 */
//================================================================================================//
#if (APP_MONITOR_FILE_USAGE > 0)
FILE_t *moni_fopen(const ch_t *path, const ch_t *mode)
{
      void *file = NULL;

      if (moni) {
            while (TakeRecMutex(moni->mtx, MTX_BLOCK_TIME) != OS_OK);

            struct taskData *taskInfo = ListGetItemDataByID(moni->tasks, (u32_t)TaskGetCurrentTaskHandle());

            if (taskInfo) {
                  /* find empty file slot */
                  u32_t block = 0;

                  while (block < FLE_BLOCK_COUNT) {
                        if (taskInfo->fblock[block]) {

                              /* find empty file slot in block */
                              for (u8_t slot = 0; slot < FLE_OPN_IN_BLOCK; slot++) {

                                    if (taskInfo->fblock[block]->full == TRUE) {
                                          break;
                                    }

                                    struct fileSlot *fslot = &taskInfo->fblock[block]->fslot[slot];

                                    if (fslot->file == NULL) {
                                          fslot->file = vfs_fopen(path, mode);

                                          if (fslot->file) {
                                                file = fslot->file;
                                          }

                                          if (slot == FLE_OPN_IN_BLOCK - 1)
                                                taskInfo->fblock[block]->full = TRUE;

                                          goto moni_fopen_end;
                                    }
                              }

                              block++;
                        } else {
                              /* create new block */
                              taskInfo->fblock[block] = calloc(1, sizeof(struct fileBlock));

                              if (taskInfo->fblock[block] == NULL) {
                                    block++;
                              }
                        }
                  }
            }

            moni_fopen_end:
            GiveRecMutex(moni->mtx);
      }

      return file;
}
#endif


//================================================================================================//
/**
 * @brief Function close opened file
 *
 * @param *file               pinter to file
 *
 * @retval STD_RET_OK         file closed successfully
 * @retval STD_RET_ERROR      file not closed
 */
//================================================================================================//
#if (APP_MONITOR_FILE_USAGE > 0)
stdRet_t moni_fclose(FILE_t *file)
{
      stdRet_t status = STD_RET_ERROR;

      if (moni) {
            while (TakeRecMutex(moni->mtx, MTX_BLOCK_TIME) != OS_OK);

            struct taskData *taskInfo = ListGetItemDataByID(moni->tasks, (u32_t)TaskGetCurrentTaskHandle());

            if (taskInfo) {
                  /* find empty file slot */
                  u32_t block = 0;

                  while (block < FLE_BLOCK_COUNT) {
                        if (taskInfo->fblock[block]) {

                              /* find opened file */
                              for (u8_t slot = 0; slot < FLE_OPN_IN_BLOCK; slot++) {

                                    struct fileSlot *fslot = &taskInfo->fblock[block]->fslot[slot];

                                    if (fslot->file == file) {
                                          status = vfs_fclose(file);

                                          if (status == STD_RET_OK) {
                                                fslot->file = NULL;

                                                taskInfo->fblock[block]->full = FALSE;

                                                if (block != 0) {
                                                      /* check if block is empty */
                                                      for (u8_t i = 0; i < MEM_ADR_IN_BLOCK; i++) {
                                                            if (taskInfo->fblock[block]->fslot[i].file != NULL)
                                                                  goto moni_fclose_end;
                                                      }

                                                      /* block is empty - freeing memory */
                                                      free(taskInfo->fblock[block]);
                                                      taskInfo->fblock[block] = NULL;
                                                }
                                          }

                                          goto moni_fclose_end;
                                    }
                              }

                              block++;
                        }
                  }
            }

            moni_fclose_end:
            GiveRecMutex(moni->mtx);
      }

      return status;
}
#endif


//================================================================================================//
/**
 * @brief Function open selected directory
 *
 * @param *name         directory path
 *
 * @retval NULL if file can't be created
 */
//================================================================================================//
#if (APP_MONITOR_FILE_USAGE > 0)
DIR_t *moni_opendir(const ch_t *path)
{
      void *dir = NULL;

      if (moni) {
            while (TakeRecMutex(moni->mtx, MTX_BLOCK_TIME) != OS_OK);

            struct taskData *taskInfo = ListGetItemDataByID(moni->tasks, (u32_t)TaskGetCurrentTaskHandle());

            if (taskInfo) {
                  /* find empty file slot */
                  u32_t block = 0;

                  while (block < DIR_BLOCK_COUNT) {
                        if (taskInfo->dblock[block]) {

                              /* find empty dir slot in block */
                              for (u8_t slot = 0; slot < DIR_OPN_IN_BLOCK; slot++) {

                                    if (taskInfo->dblock[block]->full == TRUE) {
                                          break;
                                    }

                                    struct dirSlot *dslot = &taskInfo->dblock[block]->dslot[slot];

                                    if (dslot->dir == NULL) {
                                          dslot->dir = vfs_opendir(path);

                                          if (dslot->dir) {
                                                dir = dslot->dir;
                                          }

                                          if (slot == DIR_OPN_IN_BLOCK - 1)
                                                taskInfo->dblock[block]->full = TRUE;

                                          goto moni_opendir_end;
                                    }
                              }

                              block++;
                        } else {
                              /* create new block */
                              taskInfo->dblock[block] = calloc(1, sizeof(struct dirBlock));

                              if (taskInfo->dblock[block] == NULL) {
                                    block++;
                              }
                        }
                  }
            }

            moni_opendir_end:
            GiveRecMutex(moni->mtx);
      }

      return dir;
}
#endif


//================================================================================================//
/**
 * @brief Function close opened directory
 *
 * @param *DIR                pinter to directory
 *
 * @retval STD_RET_OK         file closed successfully
 * @retval STD_RET_ERROR      file not closed
 */
//================================================================================================//
#if (APP_MONITOR_FILE_USAGE > 0)
extern stdRet_t moni_closedir(DIR_t *dir)
{
      stdRet_t status = STD_RET_ERROR;

      if (moni) {
            while (TakeRecMutex(moni->mtx, MTX_BLOCK_TIME) != OS_OK);

            struct taskData *taskInfo = ListGetItemDataByID(moni->tasks, (u32_t)TaskGetCurrentTaskHandle());

            if (taskInfo) {
                  /* find empty file slot */
                  u32_t block = 0;

                  while (block < DIR_BLOCK_COUNT) {
                        if (taskInfo->dblock[block]) {

                              /* find opened file */
                              for (u8_t slot = 0; slot < DIR_OPN_IN_BLOCK; slot++) {

                                    struct dirSlot *dslot = &taskInfo->dblock[block]->dslot[slot];

                                    if (dslot->dir == dir) {
                                          status = vfs_closedir(dir);

                                          if (status == STD_RET_OK) {
                                                dslot->dir = NULL;

                                                taskInfo->dblock[block]->full = FALSE;

                                                if (block != 0) {
                                                      /* check if block is empty */
                                                      for (u8_t i = 0; i < MEM_ADR_IN_BLOCK; i++) {
                                                            if (taskInfo->dblock[block]->dslot[i].dir != NULL)
                                                                  goto moni_closedir_end;
                                                      }

                                                      /* block is empty - freeing memory */
                                                      free(taskInfo->dblock[block]);
                                                      taskInfo->dblock[block] = NULL;
                                                }
                                          }

                                          goto moni_closedir_end;
                                    }
                              }

                              block++;
                        }
                  }
            }

            moni_closedir_end:
            GiveRecMutex(moni->mtx);
      }

      return status;
}
#endif


//================================================================================================//
/**
 * @brief Function called after task go to ready state
 */
//================================================================================================//
#if (APP_MONITOR_CPU_LOAD > 0)
void moni_TaskSwitchedIn(void)
{
      cpuctl_ClearTimeStatCnt();
}
#endif


//================================================================================================//
/**
 * @brief Function called when task go out ready state
 */
//================================================================================================//
#if (APP_MONITOR_CPU_LOAD > 0)
void moni_TaskSwitchedOut(void)
{
      u16_t  cnt     = cpuctl_GetTimeStatCnt();//TIM2->CNT;
      task_t taskhdl = TaskGetCurrentTaskHandle();
      u32_t  tmp     = (u32_t)TaskGetTag(taskhdl) + cnt;
      TaskSetTag(taskhdl, (void*)tmp);
}
#endif


#ifdef __cplusplus
}
#endif

/*==================================================================================================
                                            End of file
==================================================================================================*/