#ifndef VFS_H_
#define VFS_H_
/*=============================================================================================*//**
@file    vfs.h

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
#include "systypes.h"
#include "memman.h"


/*==================================================================================================
                                 Exported symbolic constants/macros
==================================================================================================*/
/* USER CFG: memory management, free memory */
#define VFS_FREE(ptr)               mm_free(ptr)

/* USER CFG: memory management, memory allocation */
#define VFS_CALLOC(nmemb, msize)    mm_calloc(nmemb, msize)

/* USER CFG: memory management, memory allocation */
#define VFS_MALLOC(size)            mm_malloc(size)


/* set position equal to offset bytes */
#define VFS_SEEK_SET    0

/* set position to current location plus offset */
#define VFS_SEEK_CUR    1

/* set position to EOF plus offset */
#define VFS_SEEK_END    2


/* translate functions to STDC */
#ifndef SEEK_SET
#define SEEK_SET                          VFS_SEEK_SET
#endif

#ifndef SEEK_CUR
#define SEEK_CUR                          VFS_SEEK_CUR
#endif

#ifndef SEEK_END
#define SEEK_END                          VFS_SEEK_END
#endif

#define mount(path, fs_cfgPtr)            vfs_mount(path, fs_cfgPtr)
#define umount(path)                      vfs_umount(path)
#define getmntentry(item, mntentPtr)      vfs_getmntentry(item, mntentPtr)
#define mknod(path, drv_cfgPtr)           vfs_mknod(path, drv_cfgPtr)
#define mkdir(path)                       vfs_mkdir(path)
#define opendir(path)                     vfs_opendir(path)
#define closedir(dir)                     vfs_closedir(dir)
#define readdir(dir)                      vfs_readdir(dir)
#define remove(path)                      vfs_remove(path)
#define rename(oldName, newName)          vfs_rename(oldName, newName)
#define chmod(path, mode)                 vfs_chmod(path, mode)
#define chown(path, owner, group)         vfs_chown(path, owner, group)
#define stat(path, statPtr)               vfs_stat(path, statPtr)
#define statfs(path, statfsPtr)           vfs_statfs(path, statfsPtr)
#define fopen(path, mode)                 vfs_fopen(path, mode)
#define fclose(file)                      vfs_fclose(file)
#define fwrite(ptr, isize, nitems, file)  vfs_fwrite(ptr, isize, nitems, file)
#define fread(ptr, isize, nitems, file)   vfs_fread(ptr, isize, nitems, file)
#define fseek(file, offset, mode)         vfs_fseek(file, offset, mode)
#define ftell(file)                       vfs_ftell(file)
#define ioctl(file, rq, data)             vfs_ioctl(file, rq, data)
#define fstat(file, statPtr)              vfs_fstat(file, stat)

/*==================================================================================================
                                  Exported types, enums definitions
==================================================================================================*/
struct vfs_stat {
     u32_t  st_dev;           /* ID of device containing file */
     u32_t  st_rdev;          /* device ID (if special file) */
     u32_t  st_mode;          /* protection */
     u32_t  st_uid;           /* user ID of owner */
     u32_t  st_gid;           /* group ID of owner */
     size_t st_size;          /* total size, in bytes */
     u32_t  st_mtime;         /* time of last modification */
};

struct vfs_statfs {
      u32_t f_type;           /* file system type */
      u32_t f_blocks;         /* total blocks */
      u32_t f_bfree;          /* free blocks */
      u32_t f_files;          /* total file nodes in FS */
      u32_t f_ffree;          /* free file nodes in FS */
      const ch_t *fsname;     /* FS name */
};

/* Structure describing a mount table entry.  */
struct vfs_mntent {
      ch_t *mnt_fsname;       /* device or server for filesystem.  */
      ch_t *mnt_dir;          /* directory mounted on.  */
      u32_t total;            /* device total size */
      u32_t free;             /* device free */
};

struct vfs_drvcfg {
      u32_t    dev;
      u32_t    part;
      stdRet_t (*f_open )(devx_t dev, fd_t part);
      stdRet_t (*f_close)(devx_t dev, fd_t part);
      size_t   (*f_write)(devx_t dev, fd_t part, void *src, size_t size, size_t nitems, size_t seek);
      size_t   (*f_read )(devx_t dev, fd_t part, void *dst, size_t size, size_t nitems, size_t seek);
      stdRet_t (*f_ioctl)(devx_t dev, fd_t part, IORq_t iorq, void *data);
};

struct vfs_fscfg {
      u32_t     dev;
      stdRet_t  (*f_init   )(devx_t dev);
      stdRet_t  (*f_open   )(devx_t dev, fd_t *fd, size_t *seek, const ch_t *path, const ch_t *mode);
      stdRet_t  (*f_close  )(devx_t dev, fd_t fd);
      size_t    (*f_write  )(devx_t dev, fd_t fd, void *src, size_t size, size_t nitems, size_t seek);
      size_t    (*f_read   )(devx_t dev, fd_t fd, void *dst, size_t size, size_t nitems, size_t seek);
      stdRet_t  (*f_ioctl  )(devx_t dev, fd_t fd, IORq_t iroq, void *data);
      stdRet_t  (*f_fstat  )(devx_t dev, fd_t fd, struct vfs_stat *stat);
      stdRet_t  (*f_mkdir  )(devx_t dev, const ch_t *path);
      stdRet_t  (*f_mknod  )(devx_t dev, const ch_t *path, struct vfs_drvcfg *dcfg);
      stdRet_t  (*f_opendir)(devx_t dev, const ch_t *path, DIR_t *dir);
      stdRet_t  (*f_remove )(devx_t dev, const ch_t *path);
      stdRet_t  (*f_rename )(devx_t dev, const ch_t *oldName, const ch_t *newName);
      stdRet_t  (*f_chmod  )(devx_t dev, const ch_t *path, u32_t mode);
      stdRet_t  (*f_chown  )(devx_t dev, const ch_t *path, u16_t owner, u16_t group);
      stdRet_t  (*f_stat   )(devx_t dev, const ch_t *path, struct vfs_stat *stat);
      stdRet_t  (*f_statfs )(devx_t dev, struct vfs_statfs *statfs);
      stdRet_t  (*f_release)(devx_t dev);
};


/*==================================================================================================
                                     Exported function prototypes
==================================================================================================*/
extern stdRet_t vfs_init(void);
extern stdRet_t vfs_mount(const ch_t *path, struct vfs_fscfg *mountcfg);
extern stdRet_t vfs_umount(const ch_t *path);
extern stdRet_t vfs_getmntentry(size_t item, struct vfs_mntent *mntent);
extern stdRet_t vfs_mknod(const ch_t *path, struct vfs_drvcfg *drvcfg);
extern stdRet_t vfs_mkdir(const ch_t *path);
extern DIR_t   *vfs_opendir(const ch_t *path);
extern stdRet_t vfs_closedir(DIR_t *dir);
extern dirent_t vfs_readdir(DIR_t *dir);
extern stdRet_t vfs_remove(const ch_t *path);
extern stdRet_t vfs_rename(const ch_t *oldName, const ch_t *newName);
extern stdRet_t vfs_chmod(const ch_t *path, u16_t mode);
extern stdRet_t vfs_chown(const ch_t *path, u16_t owner, u16_t group);
extern stdRet_t vfs_stat(const ch_t *path, struct vfs_stat *stat);
extern stdRet_t vfs_statfs(const ch_t *path, struct vfs_statfs *statfs);
extern FILE_t  *vfs_fopen(const ch_t *path, const ch_t *mode);
extern stdRet_t vfs_fclose(FILE_t *file);
extern size_t   vfs_fwrite(void *ptr, size_t size, size_t nitems, FILE_t *file);
extern size_t   vfs_fread(void *ptr, size_t size, size_t nitems, FILE_t *file);
extern stdRet_t vfs_fseek(FILE_t *file, i32_t offset, i32_t mode);
extern i32_t    vfs_ftell(FILE_t *file);
extern stdRet_t vfs_ioctl(FILE_t *file, IORq_t rq, void *data);
extern stdRet_t vfs_fstat(FILE_t *file, struct vfs_stat *stat);


#ifdef __cplusplus
}
#endif

#endif /* VFS_H_ */
/*==================================================================================================
                                            End of file
==================================================================================================*/
