/*=============================================================================================*//**
@file    procfs.c

@author  Daniel Zorychta

@brief   This file support process file system (procfs)

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
#include "procfs.h"
#include "sysdrv.h"
#include "print.h"
#include "dlist.h"
#include <string.h>


/*==================================================================================================
                                  Local symbolic constants/macros
==================================================================================================*/
/* task ID string length (8B name + \0). ID is 32b hex number converted to string */
#define TASK_ID_STR_LEN                   9


#define DIR_TASKID_STR                    "taskid"
#define DIR_TASKNAME_STR                  "taskname"
#define TASK_FILE_NAME_STR                "name"
#define TASK_FILE_PRIO_STR                "priority"
#define TASK_FILE_FREESTACK_STR           "freestack"
#define TASK_FILE_USEDMEM_STR             "usedmem"
#define TASK_FILE_OPENFILES_STR           "openfiles"
#define TASK_FILE_CPULOAD_STR             "cpuload"


#define MTX_BLOCK_TIME                    10


/*==================================================================================================
                                   Local types, enums definitions
==================================================================================================*/
enum taskInfFile {
      TASK_FILE_NAME,
      TASK_FILE_PRIO,
      TASK_FILE_FREESTACK,
      TASK_FILE_USEDMEM,
      TASK_FILE_OPENFILES,
      TASK_FILE_CPULOAD,
      COUNT_OF_TASK_FILE,
      TASK_FILE_NONE
};


struct procmem {
      list_t *flist;
      u32_t   idcnt;
      mutex_t mtx;
};


struct fileinfo {
      task_t taskHdl;         /* task handle */
      u8_t   taskFile;        /* task info file */
};


/*==================================================================================================
                                      Local function prototypes
==================================================================================================*/
static stdRet_t procfs_closedir_freedd(devx_t dev, DIR_t *dir);
static stdRet_t procfs_closedir_noop(devx_t dev, DIR_t *dir);
static dirent_t procfs_readdir_root(devx_t dev, DIR_t *dir);
static dirent_t procfs_readdir_taskname(devx_t dev, DIR_t *dir);
static dirent_t procfs_readdir_taskid(devx_t dev, DIR_t *dir);
static dirent_t procfs_readdir_taskid_n(devx_t dev, DIR_t *dir);


/*==================================================================================================
                                      Local object definitions
==================================================================================================*/
static struct procmem *procmem;


/*==================================================================================================
                                     Exported object definitions
==================================================================================================*/


/*==================================================================================================
                                        Function definitions
==================================================================================================*/

//================================================================================================//
/**
 * @brief Function initialize FS
 *
 * @param[in] dev           device number
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_init(devx_t dev)
{
      (void)dev;

      stdRet_t status = STD_RET_OK;

      if (procmem == NULL) {
            status = STD_RET_ERROR;

            if ((procmem = calloc(1, sizeof(struct procmem))) != NULL) {
                  procmem->flist = ListCreate();
                  CreateMutex(procmem->mtx);

                  if (procmem->flist && procmem->mtx) {
                        status = STD_RET_OK;
                  } else {
                        if (procmem->flist) {
                              ListDelete(procmem->flist);
                        }

                        if (procmem->mtx) {
                              DeleteMutex(procmem->mtx);
                        }

                        free(procmem);
                        procmem = NULL;
                  }
            }
      }

      return status;
}


//================================================================================================//
/**
 * @brief Release file system
 *
 * @param[in] dev           device number
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_release(devx_t dev)
{
      (void)dev;

      stdRet_t status = STD_RET_ERROR;

      if (procmem) {
            while (TakeMutex(procmem->mtx, MTX_BLOCK_TIME) != OS_OK);
            TaskSuspendAll();
            GiveMutex(procmem->mtx);
            DeleteMutex(procmem->mtx);
            ListDelete(procmem->flist);
            free(procmem);
            procmem = NULL;
            TaskResumeAll();

            status = STD_RET_OK;
      }

      return status;
}


//================================================================================================//
/**
 * @brief Function open selected file
 *
 * @param[in]   dev          device number
 * @param[out] *fd           file descriptor
 * @param[out] *seek         file position
 * @param[in]  *path         file name
 * @param[in]  *mode         file mode
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_open(devx_t dev, fd_t *fd, size_t *seek, const ch_t *path, const ch_t *mode)
{
      (void)dev;

      stdRet_t status = STD_RET_ERROR;

      if (path && mode && procmem) {
            if (strncmp(mode, "r", 2) != 0) {
                  goto procfs_open_end;
            }

            struct taskstat taskdata;

            if (strncmp(path, "/"DIR_TASKID_STR"/", 8) == 0) {
                  path = strchr(path + 1, '/') + 1;

                  if (path == NULL) {
                        goto procfs_open_end;
                  }

                  u32_t taskHdl = 0;

                  path = atoi((ch_t*)path, 16, (i32_t*)&taskHdl);

                  if ((status = moni_GetTaskHdlStat((task_t)taskHdl, &taskdata)) != STD_RET_OK) {
                        goto procfs_open_end;
                  }

                  if ((path = strrchr(path, '/')) == NULL) {
                        goto procfs_open_end;
                  } else {
                        path++;
                  }

                  struct fileinfo *fileInf = calloc(1, sizeof(struct fileinfo));

                  if (fileInf) {
                        fileInf->taskHdl = (task_t)taskHdl;

                        if (strcmp((ch_t*)path, TASK_FILE_NAME_STR) == 0) {
                              fileInf->taskFile = TASK_FILE_NAME;
                        } else if (strcmp((ch_t*)path, TASK_FILE_CPULOAD_STR) == 0) {
                              fileInf->taskFile = TASK_FILE_CPULOAD;
                        } else if (strcmp((ch_t*)path, TASK_FILE_FREESTACK_STR) == 0) {
                              fileInf->taskFile = TASK_FILE_FREESTACK;
                        } else if (strcmp((ch_t*)path, TASK_FILE_OPENFILES_STR) == 0) {
                              fileInf->taskFile = TASK_FILE_OPENFILES;
                        } else if (strcmp((ch_t*)path, TASK_FILE_PRIO_STR) == 0) {
                              fileInf->taskFile = TASK_FILE_PRIO;
                        } else if (strcmp((ch_t*)path, TASK_FILE_USEDMEM_STR) == 0) {
                              fileInf->taskFile = TASK_FILE_USEDMEM;
                        } else {
                              free(fileInf);
                              goto procfs_open_end;
                        }

                        while (TakeMutex(procmem->mtx, MTX_BLOCK_TIME) != OS_OK);
                        if (ListAddItem(procmem->flist, procmem->idcnt, fileInf) == 0) {
                              *fd   = procmem->idcnt++;
                              *seek = 0;

                              status = STD_RET_OK;
                        }
                        GiveMutex(procmem->mtx);
                  }
            } else if (strncmp(path, "/"DIR_TASKNAME_STR"/", 8) == 0) {
                  path = strrchr(path, '/');

                  if (path == NULL) {
                        goto procfs_open_end;
                  } else {
                        path++;
                  }

                  u16_t n = moni_GetTaskCount();
                  u16_t i = 0;

                  while (n-- && moni_GetTaskStat(i, &taskdata) == STD_RET_OK) {
                        if (strcmp(path , taskdata.taskName) == 0) {
                              struct fileinfo *fileInf = calloc(1, sizeof(struct fileinfo));

                              if (fileInf) {
                                    fileInf->taskHdl = taskdata.taskHdl;
                                    fileInf->taskFile = TASK_FILE_NONE;

                                    while (TakeMutex(procmem->mtx, MTX_BLOCK_TIME) != OS_OK);
                                    if (ListAddItem(procmem->flist, procmem->idcnt, fileInf) == 0) {
                                          *fd   = procmem->idcnt++;
                                          *seek = 0;

                                          status = STD_RET_OK;
                                    }
                                    GiveMutex(procmem->mtx);
                              }

                              break;
                        }
                  }
            }
      }

      procfs_open_end:
      return status;
}


//================================================================================================//
/**
 * @brief Close file
 *
 * @param[in]  dev          device number
 * @param[in] *fd           file descriptor
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_close(devx_t dev, fd_t fd)
{
      (void)dev;

      stdRet_t status = STD_RET_ERROR;

      if (procmem) {
            while (TakeMutex(procmem->mtx, MTX_BLOCK_TIME) != OS_OK);

            if (ListRmItemByID(procmem->flist, fd) == 0) {
                  status = STD_RET_OK;
            }

            GiveMutex(procmem->mtx);
      }

      return status;
}


//================================================================================================//
/**
 * @brief Write data file
 *
 * @param[in]  dev          device number
 * @param[in] *fd           file descriptor
 * @param[in] *src          data source
 * @param[in]  size         item size
 * @param[in]  nitems       item count
 * @param[in]  seek         file position
 *
 * @return written nitems
 */
//================================================================================================//
size_t procfs_write(devx_t dev, fd_t fd, void *src, size_t size, size_t nitems, size_t seek)
{
      (void)dev;
      (void)fd;
      (void)src;
      (void)size;
      (void)nitems;
      (void)seek;

      return 0;
}


//================================================================================================//
/**
 * @brief Read data files
 *
 * @param[in]   dev          device number
 * @param[in]  *fd           file descriptor
 * @param[out] *dst          data destination
 * @param[in]   size         item size
 * @param[in]   nitems       item count
 * @param[in]   seek         file position
 *
 * @retval read nitems
 */
//================================================================================================//
size_t procfs_read(devx_t dev, fd_t fd, void *dst, size_t size, size_t nitems, size_t seek)
{
      (void)dev;

      size_t n = 0;

      if (dst && procmem) {
            while (TakeMutex(procmem->mtx, MTX_BLOCK_TIME) != OS_OK);
            struct fileinfo *fileInf = ListGetItemDataByID(procmem->flist, fd);
            GiveMutex(procmem->mtx);

            if (fileInf){
                  if (fileInf->taskFile >= COUNT_OF_TASK_FILE) {
                        goto procfs_read_end;
                  }

                  struct taskstat taskInfo;

                  if (moni_GetTaskHdlStat(fileInf->taskHdl, &taskInfo) != STD_RET_OK) {
                        goto procfs_read_end;
                  }

                  ch_t  data[12] = {0};
                  ch_t *dataPtr = data;
                  u8_t  dataSize = 0;

                  switch (fileInf->taskFile) {
                  case TASK_FILE_CPULOAD:
                        dataSize = snprintf(data, sizeof(data), "%u",
                                            (taskInfo.cpuUsage * 100)/taskInfo.cpuUsageTotal);
                        break;

                  case TASK_FILE_FREESTACK:
                        dataSize = snprintf(data, sizeof(data), "%u", taskInfo.taskFreeStack);
                        break;

                  case TASK_FILE_NAME:
                        dataSize = strlen(taskInfo.taskName);
                        dataPtr  = taskInfo.taskName;
                        break;

                  case TASK_FILE_OPENFILES:
                        dataSize = snprintf(data, sizeof(data), "%u", taskInfo.fileUsage);
                        break;

                  case TASK_FILE_PRIO:
                        dataSize = snprintf(data, sizeof(data), "%d", taskInfo.taskPriority);
                        break;

                  case TASK_FILE_USEDMEM:
                        dataSize = snprintf(data, sizeof(data), "%u", taskInfo.memUsage);
                        break;
                  }

                  if (seek > dataSize) {
                        n = 0;
                  } else {
                        if (dataSize - seek < size * nitems) {
                              n = dataSize - seek;
                              strncpy(dst, dataPtr + seek, n);
                        } else {
                              n = size * nitems;
                              strncpy(dst, dataPtr + seek, n);
                        }
                  }
            }
      }

      procfs_read_end:
      return n;
}


//================================================================================================//
/**
 * @brief Control file
 *
 * @param[in]      dev          device number
 * @param[in]      fd           file descriptor
 * @param[in]      iorq         request
 * @param[in,out] *data         data
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_ioctl(devx_t dev, fd_t fd, IORq_t iorq, void *data)
{
      (void)dev;
      (void)fd;
      (void)iorq;
      (void)data;

      return STD_RET_ERROR;
}


//================================================================================================//
/**
 * @brief Statistics of opened file
 *
 * @param[in]   dev          device number
 * @param[in]  *fd           file descriptor
 * @param[out] *stat         output statistics
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_fstat(devx_t dev, fd_t fd, struct vfs_stat *stat)
{
      (void)dev;

      stdRet_t status = STD_RET_ERROR;

      if (stat && procmem) {
            while (TakeMutex(procmem->mtx, MTX_BLOCK_TIME) != OS_OK);
            struct fileinfo *fileInf = ListGetItemDataByID(procmem->flist, fd);
            GiveMutex(procmem->mtx);

            if (fileInf){
                  if (fileInf->taskFile >= COUNT_OF_TASK_FILE) {
                        goto procfs_fstat_end;
                  }

                  struct taskstat taskInfo;

                  if (moni_GetTaskHdlStat(fileInf->taskHdl, &taskInfo) != STD_RET_OK) {
                        goto procfs_fstat_end;
                  }

                  stat->st_dev   = 0;
                  stat->st_mode  = 0444;
                  stat->st_mtime = 0;
                  stat->st_rdev  = 0;
                  stat->st_size  = 0;
                  stat->st_gid   = 0;
                  stat->st_uid   = 0;

                  ch_t data[12] = {0};

                  switch (fileInf->taskFile) {
                  case TASK_FILE_CPULOAD:
                        stat->st_size = 3;
                        break;

                  case TASK_FILE_FREESTACK:
                        stat->st_size = snprintf(data, sizeof(data), "%u", taskInfo.taskFreeStack);
                        break;

                  case TASK_FILE_NAME:
                        stat->st_size = strlen(taskInfo.taskName);
                        break;

                  case TASK_FILE_OPENFILES:
                        stat->st_size = snprintf(data, sizeof(data), "%u", taskInfo.fileUsage);
                        break;

                  case TASK_FILE_PRIO:
                        stat->st_size = snprintf(data, sizeof(data), "%d", taskInfo.taskPriority);
                        break;

                  case TASK_FILE_USEDMEM:
                        stat->st_size = snprintf(data, sizeof(data), "%u", taskInfo.memUsage);
                        break;
                  }

                  status = STD_RET_OK;
            }
      }

      procfs_fstat_end:
      return status;
}


//================================================================================================//
/**
 * @brief Create directory
 *
 * @param[in]  dev          device number
 * @param[in] *path         directory path
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_mkdir(devx_t dev, const ch_t *path)
{
      (void)dev;
      (void)path;

      return STD_RET_ERROR;
}


//================================================================================================//
/**
 * @brief Create device node
 *
 * @param[in]  dev          device number
 * @param[in] *path         node path
 * @param[in] *dcfg         device configuration
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_mknod(devx_t dev, const ch_t *path, struct vfs_drvcfg *dcfg)
{
      (void)dev;
      (void)path;
      (void)dcfg;

      return STD_RET_ERROR;
}


//================================================================================================//
/**
 * @brief Opens directory
 *
 * @param[in]   dev          device number
 * @param[in]  *path         directory path
 * @param[out] *dir          directory object to fill
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_opendir(devx_t dev, const ch_t *path, DIR_t *dir)
{
      (void)dev;

      stdRet_t status = STD_RET_ERROR;

      if (path && dir) {
            dir->seek = 0;

            if (strcmp(path, "/") == 0) {
                  dir->dd    = NULL;
                  dir->items = 2;
                  dir->rddir = procfs_readdir_root;
                  dir->cldir = procfs_closedir_noop;
                  status     = STD_RET_OK;
            } else if (strcmp(path, "/"DIR_TASKNAME_STR"/") == 0) {
                  dir->dd    = NULL;
                  dir->items = moni_GetTaskCount();
                  dir->rddir = procfs_readdir_taskname;
                  dir->cldir = procfs_closedir_noop;
                  status     = STD_RET_OK;
            } else if (strcmp(path, "/"DIR_TASKID_STR"/") == 0) {
                  dir->dd    = calloc(TASK_ID_STR_LEN, sizeof(ch_t));
                  dir->items = moni_GetTaskCount();
                  dir->rddir = procfs_readdir_taskid;
                  dir->cldir = procfs_closedir_freedd;
                  status     = STD_RET_OK;
            } else if (strncmp(path, "/"DIR_TASKID_STR"/", strlen(DIR_TASKID_STR) + 2) == 0) {
                  path = strchr(path + 1, '/') + 1;

                  if (path == NULL) {
                        goto procfs_opendir_end;
                  }

                  u32_t taskHdl = 0;

                  path = atoi((ch_t*)path, 16, (i32_t*)&taskHdl);

                  if (!((*path == '/' && *(path + 1) == '\0') || *path == '\0')) {
                        goto procfs_opendir_end;
                  }

                  struct taskstat taskdata;

                  if ((status = moni_GetTaskHdlStat((task_t)taskHdl, &taskdata)) == STD_RET_OK) {
                        dir->dd    = (void*)taskHdl;
                        dir->items = COUNT_OF_TASK_FILE;
                        dir->rddir = procfs_readdir_taskid_n;
                        dir->cldir = procfs_closedir_noop;
                        status     = STD_RET_OK;
                  }
            }
      }

      procfs_opendir_end:
      return status;
}


//================================================================================================//
/**
 * @brief Function close opened dir (is used when dd contains pointer to allocated block)
 *
 * @param[in]   dev          device number
 * @param[out] *dir          directory object
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
static stdRet_t procfs_closedir_freedd(devx_t dev, DIR_t *dir)
{
      (void)dev;

      stdRet_t status = STD_RET_ERROR;

      if (dir) {
            if (dir->dd) {
                  free(dir->dd);
                  dir->dd = NULL;

                  status = STD_RET_OK;
            }
      }

      return status;
}


//================================================================================================//
/**
 * @brief Function close opened dir (is used when dd contains data which cannot be freed)
 *
 * @param[in]   dev          device number
 * @param[out] *dir          directory object
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
static stdRet_t procfs_closedir_noop(devx_t dev, DIR_t *dir)
{
      (void)dev;
      (void)dir;

      return STD_RET_OK;
}


//================================================================================================//
/**
 * @brief Remove file
 *
 * @param[in]   dev          device number
 * @param[out] *path         file path
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_remove(devx_t dev, const ch_t *path)
{
      (void)dev;
      (void)path;

      return STD_RET_ERROR;
}


//================================================================================================//
/**
 * @brief Rename file
 *
 * @param[in]  dev          device number
 * @param[in] *oldName      old file name
 * @param[in] *newName      new file name
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_rename(devx_t dev, const ch_t *oldName, const ch_t *newName)
{
      (void)dev;
      (void)oldName;
      (void)newName;

      return STD_RET_ERROR;
}


//================================================================================================//
/**
 * @brief Change file mode
 *
 * @param[in]  dev          device number
 * @param[in] *path         file path
 * @param[in]  mode         new mode
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_chmod(devx_t dev, const ch_t *path, u32_t mode)
{
      (void)dev;
      (void)path;
      (void)mode;

      return STD_RET_ERROR;
}


//================================================================================================//
/**
 * @brief Change file owner and group
 *
 * @param[in]  dev          device number
 * @param[in] *path         file path
 * @param[in]  owner        owner
 * @param[in]  group        group
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_chown(devx_t dev, const ch_t *path, u16_t owner, u16_t group)
{
      (void)dev;
      (void)path;
      (void)owner;
      (void)group;

      return STD_RET_ERROR;
}


//================================================================================================//
/**
 * @brief File statistics
 *
 * @param[in]   dev          device number
 * @param[in]  *path         file path
 * @param[out] *stat         file statistics
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_stat(devx_t dev, const ch_t *path, struct vfs_stat *stat)
{
      (void)dev;

      stdRet_t status = STD_RET_ERROR;

      if (path && stat) {
            stat->st_dev   = 0;
            stat->st_gid   = 0;
            stat->st_mode  = 0444;
            stat->st_mtime = 0;
            stat->st_rdev  = 0;
            stat->st_size  = 0;
            stat->st_uid   = 0;

            status = STD_RET_OK;
      }

      return status;
}


//================================================================================================//
/**
 * @brief File system statistics
 *
 * @param[in]   dev          device number
 * @param[out] *statfs       FS statistics
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t procfs_statfs(devx_t dev, struct vfs_statfs *statfs)
{
      (void)dev;

      stdRet_t status = STD_RET_ERROR;

      if (statfs) {
            statfs->f_bfree  = 0;
            statfs->f_blocks = 0;
            statfs->f_ffree  = 0;
            statfs->f_files  = 0;
            statfs->f_type   = 1;
            statfs->fsname   = "procfs";

            status = STD_RET_OK;
      }

      return status;
}


//================================================================================================//
/**
 * @brief Read item from opened directory
 *
 * @param[in]   dev          device number
 * @param[out] *dir          directory object
 *
 * @return directory entry
 */
//================================================================================================//
static dirent_t procfs_readdir_root(devx_t dev, DIR_t *dir)
{
      (void)dev;

      dirent_t dirent;
      dirent.name = NULL;
      dirent.size = 0;

      if (dir) {
            dirent.filetype = FILE_TYPE_DIR;
            dirent.size     = 0;

            switch (dir->seek++) {
            case 0: dirent.name = DIR_TASKID_STR;   break;
            case 1: dirent.name = DIR_TASKNAME_STR; break;
            default: break;
            }
      }

      return dirent;
}

//================================================================================================//
/**
 * @brief Read item from opened directory
 *
 * @param[in]   dev          device number
 * @param[out] *dir          directory object
 *
 * @return directory entry
 */
//================================================================================================//
static dirent_t procfs_readdir_taskname(devx_t dev, DIR_t *dir)
{
      (void)dev;

      dirent_t dirent;
      dirent.name = NULL;
      dirent.size = 0;

      struct taskstat taskdata;

      if (dir) {
            if (moni_GetTaskStat(dir->seek, &taskdata) == STD_RET_OK) {
                  dirent.filetype = FILE_TYPE_REGULAR;
                  dirent.name     = taskdata.taskName;
                  dirent.size     = 0;

                  dir->seek++;
            }
      }

      return dirent;
}


//================================================================================================//
/**
 * @brief Read item from opened directory
 *
 * @param[in]   dev          device number
 * @param[out] *dir          directory object
 *
 * @return directory entry
 */
//================================================================================================//
static dirent_t procfs_readdir_taskid(devx_t dev, DIR_t *dir)
{
      (void)dev;

      dirent_t dirent;
      dirent.name = NULL;
      dirent.size = 0;

      struct taskstat taskdata;

      if (dir) {
            if (dir->dd && dir->seek < moni_GetTaskCount()) {
                  if (moni_GetTaskStat(dir->seek, &taskdata) == STD_RET_OK) {
                        snprintf(dir->dd, TASK_ID_STR_LEN, "%x", (u32_t)taskdata.taskHdl);

                        dirent.filetype = FILE_TYPE_DIR;
                        dirent.name     = dir->dd;
                        dirent.size     = 0;

                        dir->seek++;
                  }
            }
      }

      return dirent;
}


//================================================================================================//
/**
 * @brief Read item from opened directory
 *
 * @param[in]   dev          device number
 * @param[out] *dir          directory object
 *
 * @return directory entry
 */
//================================================================================================//
static dirent_t procfs_readdir_taskid_n(devx_t dev, DIR_t *dir)
{
      (void)dev;

      dirent_t dirent;
      dirent.name = NULL;
      dirent.size = 0;

      if (dir) {
            struct taskstat taskdata;

            if (  dir->seek < COUNT_OF_TASK_FILE
               && moni_GetTaskHdlStat((task_t)dir->dd, &taskdata) == STD_RET_OK) {
                  dirent.filetype = FILE_TYPE_REGULAR;
                  dirent.size = sizeof(u32_t);

                  switch (dir->seek) {
                  case TASK_FILE_NAME     : dirent.name = TASK_FILE_NAME_STR;
                                            dirent.size = strlen(taskdata.taskName);
                                            break;
                  case TASK_FILE_PRIO     : dirent.name = TASK_FILE_PRIO_STR;      break;
                  case TASK_FILE_FREESTACK: dirent.name = TASK_FILE_FREESTACK_STR; break;
                  case TASK_FILE_USEDMEM  : dirent.name = TASK_FILE_USEDMEM_STR;   break;
                  case TASK_FILE_OPENFILES: dirent.name = TASK_FILE_OPENFILES_STR; break;
                  case TASK_FILE_CPULOAD  : dirent.name = TASK_FILE_CPULOAD_STR;   break;
                  }

                  dir->seek++;
            }
      }

      return dirent;
}


#ifdef __cplusplus
}
#endif

/*==================================================================================================
                                            End of file
==================================================================================================*/
