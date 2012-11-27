/*=============================================================================================*//**
@file    vfs.c

@author  Daniel Zorychta

@brief   This file support virtual file system

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
#include "vfs.h"
#include "system.h"
#include <string.h>


/*==================================================================================================
                                  Local symbolic constants/macros
==================================================================================================*/
#define MTX_BLOCK_TIME        100


/*==================================================================================================
                                   Local types, enums definitions
==================================================================================================*/
typedef enum {
      NODE_TYPE_UNKNOWN,
      NODE_TYPE_DIR,
      NODE_TYPE_FILE,
      NODE_TYPE_FS,
      NODE_TYPE_DRV,
} nodeType_t;

struct node {
      ch_t       *name;       /* file name */
      nodeType_t  type;       /* file type */
      u32_t       dev;        /* major device number */
      u32_t       part;       /* minor device number */
      u32_t       mode;       /* protection */
      u32_t       uid;        /* user ID of owner */
      u32_t       gid;        /* group ID of owner */
      size_t      size;       /* file size */
      u32_t       mtime;      /* time of last modification */
      void       *data;       /* file type specified data */
};

typedef struct node node_t;

struct fshdl_s {
      node_t  root;
      mutex_t mtx;
};


/*==================================================================================================
                                      Local function prototypes
==================================================================================================*/
static ch_t     *GetWordFromPath(ch_t *str, ch_t **word, size_t *length);
static node_t   *GetNode(const ch_t *path, node_t *startnode, const ch_t **extPath, i32_t deep, i32_t *nitem);
static i32_t     GetPathDeep(const ch_t *path);
static dirent_t  lfs_readdir(DIR_t *dir);
static stdRet_t  lfs_close(u32_t dev, u32_t fd);
static size_t    lfs_write(u32_t dev, u32_t fd, void *src, size_t size, size_t nitems, size_t seek);
static size_t    lfs_read(u32_t dev, u32_t fd, void *dst, size_t size, size_t nitems, size_t seek);


/*==================================================================================================
                                      Local object definitions
==================================================================================================*/
static struct fshdl_s *fs;


/*==================================================================================================
                                        Function definitions
==================================================================================================*/

//================================================================================================//
/**
 * @brief Initialize VFS module
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t vfs_init(void)
{
      stdRet_t ret = STD_RET_OK;

      if (TakeMutex(fs->mtx, MTX_BLOCK_TIME)) {
            if (fs == NULL) {
                  fs = calloc(1, sizeof(struct fshdl_s));

                  if (fs) {
                        CreateMutex(fs->mtx);
                        fs->root.data = ListCreate();

                        if (!fs->mtx || !fs->root.data) {
                              if (fs->mtx)
                                    DeleteMutex(fs->mtx);

                              if (fs->root.data)
                                    ListDestroy(fs->root.data);

                              free(fs);

                              fs = NULL;
                        } else {
                              fs->root.name = "/";
                              fs->root.size = sizeof(node_t);
                              fs->root.type = NODE_TYPE_DIR;
                        }
                  }

                  if (fs == NULL)
                        ret = STD_RET_ERROR;
            }

            GiveMutex(fs->mtx);
      }

      return ret;
}


//================================================================================================//
/**
 * @brief Function mount file system in VFS
 * External file system is mounted to empty directory. If directory is not empty mounting is not
 * possible.
 *
 * @param *path               path when dir shall be mounted
 * @param *mountcfg           pointer to description of mount FS
 *
 * @retval STD_RET_OK         mount success
 * @retval STD_RET_ERROR      mount error
 */
//================================================================================================//
stdRet_t vfs_mount(const ch_t *path, vfsmcfg_t *mountcfg)
{
      stdRet_t status = STD_RET_ERROR;

      if (TakeMutex(fs->mtx, MTX_BLOCK_TIME)) {
            if (path && mountcfg && fs) {
                  /* try parse folder name to create */
                  i32_t deep = GetPathDeep(path);

                  if (deep) {
                        /* go to target dir */
                        node_t *node = GetNode(path, &fs->root, NULL, 0, NULL);

                        /* check if target node is OK */
                        if (node) {
                              if (node->type == NODE_TYPE_DIR && ListGetItemCount(node->data) == 0) {
                                    ListDestroy(node->data);

                                    vfsmcfg_t *mcfg  = calloc(1, sizeof(vfsmcfg_t));

                                    if (mcfg) {
                                          *mcfg      = *mountcfg;
                                          node->data = mcfg;
                                          node->type = NODE_TYPE_FS;

                                          status = STD_RET_OK;
                                    }
                              }
                        }
                  }
            }

            GiveMutex(fs->mtx);
      }

      return status;
}


//================================================================================================//
/**
 * @brief Function umount dir from file system
 *
 * @param *path               dir path
 *
 * @retval STD_RET_OK         mount success
 * @retval STD_RET_ERROR      mount error
 */
//================================================================================================//
stdRet_t vfs_umount(const ch_t *path)
{
      stdRet_t status = STD_RET_ERROR;

      if (TakeMutex(fs->mtx, MTX_BLOCK_TIME)) {
            if (path && fs) {
                  /* try parse folder name to create */
                  i32_t deep = GetPathDeep(path);

                  if (deep) {
                        node_t *node = GetNode(path, &fs->root, NULL, 0, NULL);

                        /* check if target node is OK */
                        if (node) {
                              if (node->type == NODE_TYPE_FS) {
                                    list_t *dir = ListCreate();

                                    if (dir) {
                                          if (node->data)
                                                free(node->data);

                                          node->data = dir;
                                          node->type = NODE_TYPE_DIR;

                                          status = STD_RET_OK;
                                    }
                              }
                        }
                  }
            }

            GiveMutex(fs->mtx);
      }

      return status;
}


//================================================================================================//
/**
 * @brief Function create node for driver file
 *
 * @param *path               path when driver-file shall be created
 * @param *drvcfg             pointer to description of driver
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t vfs_mknod(const ch_t *path, vfsdcfg_t *drvcfg)
{
      stdRet_t status = STD_RET_ERROR;

      if (TakeMutex(fs->mtx, MTX_BLOCK_TIME)) {
            if (path && drvcfg && fs) {
                  /* try parse folder name to create */
                  i32_t deep = GetPathDeep(path);

                  if (deep) {
                        /* go to target dir */
                        node_t *node = GetNode(path, &fs->root, NULL, -1, NULL);

                        /* check if target node is OK */
                        if (node) {
                              if (node->type == NODE_TYPE_DIR) {
                                    ch_t  *drvname    = strrchr(path, '/') + 1;
                                    u32_t  drvnamelen = strlen(drvname);
                                    ch_t  *filename   = calloc(drvnamelen + 1, sizeof(ch_t));

                                    if (filename) {
                                          strcpy(filename, drvname);

                                          node_t    *dirfile = calloc(1, sizeof(node_t));
                                          vfsdcfg_t *dcfg    = calloc(1, sizeof(vfsdcfg_t));

                                          if (dirfile && dcfg) {
                                                memcpy(dcfg, drvcfg, sizeof(vfsdcfg_t));

                                                dirfile->name  = filename;
                                                dirfile->size  = sizeof(node_t) + strlen(filename)
                                                               + sizeof(vfsdcfg_t);
                                                dirfile->type  = NODE_TYPE_DRV;
                                                dirfile->data  = dcfg;
                                                dirfile->dev   = dcfg->dev;
                                                dirfile->part  = dcfg->part;

                                                /* add new drv to this folder */
                                                if (ListAddItem(node->data, dirfile) >= 0) {
                                                      status = STD_RET_OK;
                                                }
                                          }

                                          /* free memory when error */
                                          if (status == STD_RET_ERROR) {
                                                if (dirfile)
                                                      free(dirfile);

                                                if (dcfg)
                                                      free(dcfg);

                                                free(filename);
                                          }
                                    }
                              }
                        }
                  }
            }

            GiveMutex(fs->mtx);
      }

      return status;
}


//================================================================================================//
/**
 * @brief Create directory
 *
 * @param *path   path to new directory
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t vfs_mkdir(const ch_t *path)
{
      stdRet_t status = STD_RET_ERROR;

      if (TakeMutex(fs->mtx, MTX_BLOCK_TIME)) {
            if (path && fs) {
                  /* try parse folder name to create */
                  i32_t deep = GetPathDeep(path);

                  if (deep) {
                        /* go to target dir */
                        const ch_t *extPath = NULL;
                        node_t     *node    = GetNode(path, &fs->root, &extPath, -1, NULL);
                        node_t     *ifnode  = GetNode(strrchr(path, '/'), node, NULL, 0, NULL);

                        /* check if target node is OK and the same name doesn't exist */
                        if (node && !ifnode) {
                              /* internal FS */
                              if (node->type ==  NODE_TYPE_DIR) {
                                    ch_t  *dirname    = strrchr(path, '/') + 1;
                                    u32_t  dirnamelen = strlen(dirname);
                                    ch_t  *name       = calloc(dirnamelen + 1, sizeof(ch_t));

                                    /* check if name buffer is created */
                                    if (name) {
                                          strcpy(name, dirname);

                                          node_t *dir = calloc(1, sizeof(node_t));

                                          if (dir) {
                                                dir->data = ListCreate();

                                                if (dir->data) {
                                                      dir->name = (ch_t*)name;
                                                      dir->size = sizeof(node_t) + strlen(name);
                                                      dir->type = NODE_TYPE_DIR;

                                                      /* add new folder to this folder */
                                                      if (ListAddItem(node->data, dir) >= 0) {
                                                            status = STD_RET_OK;
                                                      }
                                                }
                                          }

                                          /* free memory when error */
                                          if (status == STD_RET_ERROR) {
                                                if (dir) {
                                                      if (dir->data)
                                                            ListDestroy(dir->data);

                                                      free(dir);
                                                }

                                                free(name);
                                          }
                                    }
                              /* external FS */
                              } else if (node->type ==  NODE_TYPE_FS) {
                                    if (node->data) {
                                          vfsmcfg_t *extfs = node->data;

                                          if (node->data) {
                                                if (extfs->mkdir(extfs->dev, extPath) == 0)
                                                      status = STD_RET_OK;
                                          }
                                    }
                              }
                        }
                  }
            }

            GiveMutex(fs->mtx);
      }

      return status;
}


//================================================================================================//
/**
 * @brief Function open directory
 *
 * @param *path         directory path
 *
 * @return directory object
 */
//================================================================================================//
DIR_t *vfs_opendir(const ch_t *path)
{
      DIR_t *dir = NULL;

      if (TakeMutex(fs->mtx, MTX_BLOCK_TIME)) {
            if (path && fs) {
                  dir = calloc(1, sizeof(DIR_t));

                  if (dir) {
                        /* go to target dir */
                        const ch_t *extPath = NULL;
                        node_t     *node    = GetNode(path, &fs->root, &extPath, 0, NULL);

                        if (node) {
                              switch (node->type) {
                              case NODE_TYPE_DIR:
                                    dir->items = ListGetItemCount(node->data);
                                    dir->rddir = lfs_readdir;
                                    dir->seek  = 0;
                                    dir->dd    = node;
                                    break;

                              case NODE_TYPE_FS:
                                    if (node->data) {
                                          /*
                                           * freeing DIR object because external FS create
                                           * own object internally
                                           */
                                          free(dir);
                                          dir = NULL;

                                          if (extPath == NULL) {
                                                extPath = "/";
                                          }

                                          /* open external DIR */
                                          vfsmcfg_t *extfs = node->data;

                                          if (extfs->opendir)
                                                dir = extfs->opendir(extfs->dev, extPath);
                                    }
                                    break;

                              /* Probably FILE */
                              default:
                                    free(dir);
                                    dir = NULL;
                                    break;
                              }
                        } else {
                              free(dir);
                              dir = NULL;
                        }
                  }
            }

            GiveMutex(fs->mtx);
      }

      return dir;
}


//================================================================================================//
/**
 * @brief Function close opened directory
 *
 * @param *dir          directory object
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t vfs_closedir(DIR_t *dir)
{
      stdRet_t status = STD_RET_ERROR;

      if (dir)
      {
            free(dir);
            status = STD_RET_OK;
      }

      return status;
}


//================================================================================================//
/**
 * @brief Function read next item of opened directory
 *
 * @param *dir          directory object
 *
 * @return element attributes
 */
//================================================================================================//
dirent_t vfs_readdir(DIR_t *dir)
{
      dirent_t direntry;
      direntry.name = NULL;
      direntry.size = 0;

      if (dir->rddir) {
            direntry = dir->rddir(dir);
      }

      return direntry;
}


//================================================================================================//
/**
 * @brief Remove file
 *
 * @param *patch        localization of file/directory
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t vfs_remove(const ch_t *path)
{
      stdRet_t status = STD_RET_ERROR;

      if (TakeMutex(fs->mtx, MTX_BLOCK_TIME)) {
            if (path) {
                  /* go to dir */
                  i32_t       item;
                  const ch_t *extPath  = NULL;
                  node_t     *nodebase = GetNode(path, &fs->root, &extPath, -1, NULL);
                  node_t     *nodeobj  = GetNode(strrchr(path, '/'), nodebase, NULL, 0, &item);

                  /* check if target nodes ares OK */
                  if (nodebase) {

                        if (nodeobj) {

                              /* node must be local FILE or DIR */
                              if (  nodeobj->type == NODE_TYPE_DIR
                                 || nodeobj->type == NODE_TYPE_FILE
                                 || nodeobj->type == NODE_TYPE_DRV) {

                                    /* if DIR check if is empty */
                                    if (nodeobj->type == NODE_TYPE_DIR) {

                                          if (ListGetItemCount(nodeobj->data) != 0) {
                                                goto vfs_remove_end;
                                          } else {
                                                ListDestroy(nodeobj->data);
                                                nodeobj->data = NULL;
                                          }
                                    }

                                    if (nodeobj->name)
                                          free(nodeobj->name);

                                    if (nodeobj->data)
                                          free(nodeobj->data);

                                    if (ListRmItem(nodebase->data, item) == 0)
                                          status = STD_RET_OK;
                              }
                        } else if (nodebase->type == NODE_TYPE_FS) {

                              vfsmcfg_t *ext = nodebase->data;

                              if (ext->remove) {
                                    if (ext->remove(ext->dev, extPath) == 0) {
                                          status = STD_RET_OK;
                                    }
                              }
                        }
                  }
            }

            GiveMutex(fs->mtx);
      }

      vfs_remove_end:
      return status;
}


//================================================================================================//
/**
 * @brief Rename file name
 * The implementation of rename can move files only if external FS provide functionality. Local
 * VFS can not do this.
 *
 * @param *oldName            old file name
 * @param *newName            new file name
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t vfs_rename(const ch_t *oldName, const ch_t *newName)
{
      stdRet_t status = STD_RET_ERROR;

      if (TakeMutex(fs->mtx, MTX_BLOCK_TIME)) {
            if (oldName && newName) {
                  i32_t       oldItem;
                  i32_t       newItem;
                  const ch_t *oldExtPath;
                  const ch_t *newExtPath;
                  node_t     *oldNodeBase = GetNode(oldName, &fs->root, &oldExtPath, -1, &oldItem);
                  node_t     *newNodeBase = GetNode(newName, &fs->root, &newExtPath, -1, &newItem);

                  if (oldNodeBase && newNodeBase) {
                        /* external the same FS -- move or rename operation */
                        if (  oldNodeBase == newNodeBase && oldNodeBase->type == NODE_TYPE_FS) {
                              vfsmcfg_t *ext = oldNodeBase->data;

                              if (ext) {
                                    if (ext->rename) {
                                          status = ext->rename(ext->dev, oldExtPath, newExtPath);
                                    }
                              }
                        /* internal FS, the same dir -- rename operation */
                        } else if (oldNodeBase == newNodeBase && oldNodeBase->type != NODE_TYPE_FS) {
                              ch_t   *name = calloc(1, strlen(strrchr(newName, '/') + 1));
                              node_t *node = GetNode(strrchr(oldName, '/'), newNodeBase, NULL, 0, NULL);

                              if (name && node) {
                                    strcpy(name, strrchr(newName, '/') + 1);

                                    if (node->name)
                                          free(node->name);

                                    node->name = name;

                                    status = STD_RET_OK;

                              } else {
                                    if (name)
                                          free(name);
                              }
                        /* internal FS, different dir -- move operation */
                        } else if (  oldNodeBase != newNodeBase
                                && oldNodeBase->type != NODE_TYPE_FS
                                && newNodeBase->type != NODE_TYPE_FS) {
                        /* internal and external FS; int->ext -- operation copy and remove */
                        } else if (  oldNodeBase != newNodeBase
                                && oldNodeBase->type != NODE_TYPE_FS
                                && newNodeBase->type == NODE_TYPE_FS) {
                        /* internal and external FS; ext->int -- operation copy and remove */
                        } else if (  oldNodeBase != newNodeBase
                                && oldNodeBase->type == NODE_TYPE_FS
                                && newNodeBase->type != NODE_TYPE_FS) {
                        /* external to external FS -- operation copy and remove */
                        } else if (  oldNodeBase != newNodeBase
                                && oldNodeBase->type == NODE_TYPE_FS
                                && newNodeBase->type == NODE_TYPE_FS) {
                        }
                  }
            }

            GiveMutex(fs->mtx);
      }

      return status;
}


//================================================================================================//
/**
 * @brief Function returns file/dir status
 *
 * @param *path         file/dir path
 * @param *stat         pointer to stat structure
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t vfs_stat(const ch_t *path, struct vfsstat *stat)
{
      stdRet_t status = STD_RET_ERROR;

      if (TakeMutex(fs->mtx, MTX_BLOCK_TIME)) {
            if (path && stat) {
                  const ch_t *extPath = NULL;
                  node_t     *node    = GetNode(path, &fs->root, &extPath, 0, NULL);

                  if (node) {
                        stat->st_dev   = node->dev;
                        stat->st_rdev  = node->part;
                        stat->st_gid   = node->gid;
                        stat->st_mode  = node->mode;
                        stat->st_mtime = node->mtime;
                        stat->st_size  = node->size;
                        stat->st_uid   = node->uid;

                        status = STD_RET_OK;
                  }
            }

            GiveMutex(fs->mtx);
      }

      return status;
}


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
FILE_t *vfs_fopen(const ch_t *path, const ch_t *mode)
{
      FILE_t *file = NULL;

      if (TakeMutex(fs->mtx, MTX_BLOCK_TIME)) {
            if (path && mode) {
                  file = calloc(1, sizeof(FILE_t));

                  if (file) {
                        const ch_t *extPath = NULL;
                        node_t     *node    = GetNode(path, &fs->root, &extPath, 0, NULL);

                        /* file does not exist */
                        if (node == NULL) {
                              /* create file when mode is correct */
                              if (  (strncmp("w", mode, 2) == 0) || (strncmp("w+", mode, 2) == 0)
                                 || (strncmp("a", mode, 2) == 0) || (strncmp("a+", mode, 2) == 0) ) {

                                    /* file does not exist, try to create new file */
                                    node_t *nodebase = GetNode(path, &fs->root, NULL, -1, NULL);

                                    if (nodebase) {
                                          if (nodebase->type == NODE_TYPE_DIR) {
                                                ch_t   *filename = calloc(1, strlen(strrchr(path, '/')));
                                                node_t *fnode    = calloc(1, sizeof(node_t));

                                                if (filename && fnode) {
                                                      strcpy(filename, strrchr(path, '/') + 1);

                                                      fnode->name  = filename;
                                                      fnode->data  = NULL;
                                                      fnode->dev   = 0;
                                                      fnode->gid   = 0;
                                                      fnode->mode  = 0;
                                                      fnode->mtime = 0;
                                                      fnode->part  = 0;
                                                      fnode->size  = 0;
                                                      fnode->type  = NODE_TYPE_FILE;
                                                      fnode->uid   = 0;

                                                      if (ListAddItem(nodebase->data, fnode) < 0) {
                                                            free(fnode);
                                                            free(filename);
                                                            free(file);
                                                            file = NULL;
                                                      }
                                                }
                                          } else {
                                                free(file);
                                                file = NULL;
                                          }
                                    } else {
                                          free(file);
                                          file = NULL;
                                    }
                              }
                        }

                        if (file) {
                              /* file shall exist */
                              node = GetNode(path, &fs->root, &extPath, 0, NULL);

                              if (node) {
                                    vfsdcfg_t *drv = node->data;
                                    vfsmcfg_t *ext = node->data;

                                    /* open file according to file type */
                                    switch (node->type) {
                                    case NODE_TYPE_FILE:
                                          file->dev     = 0;
                                          file->fd_part = 0;
                                          file->close   = lfs_close;
                                          file->read    = lfs_read;
                                          file->write   = lfs_write;
                                          file->ioctl   = NULL;
                                          file->seek    = 0;
                                          break;

                                    case NODE_TYPE_DRV:
                                          if (drv->open) {
                                                if (drv->open(drv->dev, drv->part) == STD_RET_OK) {
                                                      file->dev     = drv->dev;
                                                      file->fd_part = drv->part;
                                                      file->close   = drv->close;
                                                      file->ioctl   = drv->ioctl;
                                                      file->read    = drv->read;
                                                      file->write   = drv->write;
                                                      file->seek    = 0;
                                                } else {
                                                      free(file);
                                                      file = NULL;
                                                }
                                          }
                                          break;

                                    case NODE_TYPE_FS:
                                          if (ext->open) {
                                                file->fd_part = ext->open(ext->dev, extPath, mode);

                                                if (file->fd_part != 0) {
                                                      file->dev   = ext->dev;
                                                      file->close = ext->close;
                                                      file->ioctl = ext->ioctl;
                                                      file->read  = ext->read;
                                                      file->write = ext->write;
                                                      file->seek  = 0;
                                                } else {
                                                      free(file);
                                                      file = NULL;
                                                }
                                          }
                                          break;

                                    default:
                                          free(file);
                                          file = NULL;
                                          break;
                                    }

                                    /* check file mode */
                                    if (file) {
                                          /* open for reading */
                                          if (strncmp("r", mode, 2) == 0)
                                          {
                                                file->write = NULL;
                                          }
                                          /* open for writing (file need not exist) */
                                          else if (strncmp("w", mode, 2) == 0)
                                          {
                                                file->read = NULL;
                                          }
                                          /* open for appending (file need not exist) */
                                          else if (strncmp("a", mode, 2) == 0)
                                          {
                                                file->read = NULL;
                                          }
                                          /* open for reading and writing, start at beginning */
                                          else if (strncmp("r+", mode, 2) == 0)
                                          {
                                                /* nothing to change */
                                          }
                                          /* open for reading and writing (overwrite file) */
                                          else if (strncmp("w+", mode, 2) == 0)
                                          {
                                                /* nothing to change */
                                          }
                                          /* open for reading and writing (append if file exists) */
                                          else if (strncmp("a+", mode, 2) == 0)
                                          {
                                                /* nothing to change */
                                          }
                                          /* invalid mode */
                                          else
                                          {
                                                vfs_fclose(file);
                                                file = NULL;
                                          }
                                    }
                              }
                        }
                  }
            }

            GiveMutex(fs->mtx);
      }

      return file;
}


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
stdRet_t vfs_fclose(FILE_t *file)
{
      stdRet_t status = STD_RET_ERROR;

      if (file) {
            if (file->close) {
                  if (file->close(file->dev, file->fd_part) == STD_RET_OK) {
                        free(file);
                        status = STD_RET_OK;
                  }
            }
      }

      return status;
}


//================================================================================================//
/**
 * @brief Function write data to file
 *
 * @param *ptr                address to data (src)
 * @param size                item size
 * @param nitems              number of items
 * @param *file               pointer to file object
 *
 * @return STD_RET_OK or 0 if write finished successfully, otherwise > 0
 */
//================================================================================================//
size_t vfs_fwrite(void *ptr, size_t size, size_t nitems, FILE_t *file)
{
      size_t n = 0;

      if (ptr && size && nitems && file) {
            if (file->write) {
                  n = file->write(file->dev, file->fd_part, ptr, size, nitems, file->seek);
                  file->seek += n * size;
            }
      }

      return n;
}


//================================================================================================//
/**
 * @brief Function read data from file
 *
 * @param *ptr                address to data (dst)
 * @param size                item size
 * @param nitems              number of items
 * @param *file               pointer to file object
 *
 * @return number of read items
 */
//================================================================================================//
size_t vfs_fread(void *ptr, size_t size, size_t nitems, FILE_t *file)
{
      size_t n = 0;

      if (ptr && size && nitems && file) {
            if (file->read) {
                  n = file->read(file->dev, file->fd_part, ptr, size, nitems, file->seek);
                  file->seek += n * size;
            }
      }

      return n;
}


//================================================================================================//
/**
 * @brief Function set seek value
 *
 * @param *file               file object
 * @param offset              seek value
 * @param mode                seek mode DNLFIXME implement: seek mode
 *
 * @retval STD_RET_OK         seek moved successfully
 * @retval STD_RET_ERROR      error occurred
 */
//================================================================================================//
stdRet_t vfs_fseek(FILE_t *file, i32_t offset, i32_t mode)
{
      (void)mode;

      stdRet_t status = STD_RET_ERROR;

      if (file) {
            file->seek = offset;
            status     = STD_RET_OK;
      }

      return status;
}


//================================================================================================//
/**
 * @brief Function support not standard operations
 *
 * @param *file               file
 * @param rq                  request
 * @param *data               pointer to data
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//================================================================================================//
stdRet_t vfs_ioctl(FILE_t *file, IORq_t rq, void *data)
{
      stdRet_t status = STD_RET_ERROR;

      if (file) {
            if (file->ioctl) {
                  status = file->ioctl(file->dev, file->fd_part, rq, data);
            }
      }

      return status;
}


//================================================================================================//
/**
 * @brief Function read next item of opened directory
 *
 * @param *dir          directory object
 *
 * @return element attributes
 */
//================================================================================================//
static dirent_t lfs_readdir(DIR_t *dir)
{
      dirent_t dirent;
      dirent.isfile = TRUE;
      dirent.name   = NULL;
      dirent.size   = 0;

      if (dir) {
            node_t *from = dir->dd;
            node_t *node = ListGetItemData(from->data, dir->seek++);

            if (node) {
                  dirent.isfile = (node->type == NODE_TYPE_FILE || node->type == NODE_TYPE_DRV);
                  dirent.name   = node->name;
                  dirent.size   = node->size;
            }
      }

      return dirent;
}


//================================================================================================//
/**
 * @brief Function close file in LFS
 * Function return always STD_RET_OK, to close file in LFS no operation is needed.
 *
 * @param dev     device number
 * @param fd      file descriptor
 *
 * @retval STD_RET_OK
 */
//================================================================================================//
static stdRet_t lfs_close(u32_t dev, u32_t fd)
{
      (void)dev;
      (void)fd;

      return STD_RET_OK;
}


//================================================================================================//
/**
 * @brief
 */
//================================================================================================//
static size_t lfs_write(u32_t dev, u32_t fd, void *src, size_t size, size_t nitems, size_t seek)
{
      (void)dev;
      (void)fd;

      return 0;
}


//================================================================================================//
/**
 * @brief
 */
//================================================================================================//
static size_t lfs_read(u32_t dev, u32_t fd, void *dst, size_t size, size_t nitems, size_t seek)
{
      (void)dev;
      (void)fd;

      return 0;
}


//================================================================================================//
/**
 * @brief Function return pointer to word
 *
 * @param[in]  *str          string
 * @param[out] **word        pointer to word beginning
 * @param[out] *length       pointer to word length
 *
 * @return pointer to next word, otherwise NULL
 */
//================================================================================================//
static ch_t *GetWordFromPath(ch_t *str, ch_t **word, size_t *length)
{
      ch_t *bwd = NULL;
      ch_t *ewd = NULL;
      ch_t *nwd = NULL;

      if (str) {
            bwd = strchr(str, '/');

            if (bwd) {
                  ewd = strchr(bwd + 1, '/');

                  if (ewd == NULL) {
                        ewd = strchr(bwd + 1, '\0');
                        nwd = NULL;
                  } else {
                        nwd = ewd;
                  }

                  bwd++;
            }
      }

      if (word)
            *word   = bwd;

      if (length)
            *length = ewd - bwd;

      return nwd;
}


//================================================================================================//
/**
 * @brief Check path deep
 *
 * @param *path         path
 *
 * @return path deep
 */
//================================================================================================//
static i32_t GetPathDeep(const ch_t *path)
{
      u32_t deep = 0;
      const ch_t *lastpath = NULL;

      if (path[0] == '/') {
            lastpath = path++;

            while ((path = strchr(path, '/'))) {
                  lastpath = path;
                  path++;
                  deep++;
            }

            if (lastpath[1] != '\0')
                  deep++;
      }

      return deep;
}


//================================================================================================//
/**
 * @brief Function find node by path
 *
 * @param[in]  *path          path
 * @param[in]  *startnode     start node
 * @param[out] **extPath      external path begin (pointer from path)
 * @param[in]   deep          deep control
 * @param[out] *nitem         node is n-item of list which was found
 *
 * @return node
 */
//================================================================================================//
static node_t *GetNode(const ch_t *path, node_t *startnode, const ch_t **extPath, i32_t deep, i32_t *nitem)
{
      node_t *curnode = NULL;

      if (path && startnode) {
            if (startnode->type == NODE_TYPE_DIR) {
                  curnode          = startnode;
                  i32_t   dirdeep  = GetPathDeep(path);
                  ch_t   *word;
                  size_t  len;
                  i32_t   listsize;

                  while (dirdeep + deep > 0) {
                        /* get word from path */
                        path = GetWordFromPath((ch_t*)path, &word, &len);

                        /* get number of list items */
                        listsize = ListGetItemCount(curnode->data);

                        /* find that object exist */
                        i32_t i = 0;
                        while (listsize > 0) {
                              node_t *node = ListGetItemData(curnode->data, i++);

                              if (node) {
                                    if (strncmp(node->name, word, len) == 0) {
                                          curnode = node;

                                          if (nitem)
                                                *nitem = i - 1;
                                          break;
                                    }
                              } else {
                                    dirdeep = 1 - deep;
                                    break;
                              }

                              listsize--;
                        }

                        /* dir does not found or error */
                        if (listsize == 0 || curnode == NULL) {
                              curnode = NULL;
                              break;
                        }

                        /* if external system, exit */
                        if (curnode->type == NODE_TYPE_FS) {
                              if (extPath)
                                    *extPath = path;

                              break;
                        }

                        dirdeep--;
                  }
            }
      }

      return curnode;
}


#ifdef __cplusplus
}
#endif

/*==================================================================================================
                                            End of file
==================================================================================================*/
