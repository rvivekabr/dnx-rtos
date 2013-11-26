#include "system/dnxfs.h"
#include "core/printx.h"
#include "core/conv.h"
#include "core/progman.h"
#include "system/thread.h"
#define TASK_ID_STR_LEN                 9
#define U32_STR_LEN                     12
#define DIR_TASKID_STR                  "taskid"
#define DIR_TASKNAME_STR                "taskname"
#define DIR_BIN_STR                     "bin"
#define FILE_TASK_NAME_STR              "name"
#define FILE_TASK_PRIO_STR              "priority"
#define FILE_TASK_FREESTACK_STR         "freestack"
#define FILE_TASK_USEDMEM_STR           "usedmem"
#define FILE_TASK_OPENFILES_STR         "openfiles"

#define MTX_BLOCK_TIME                  10

#define SECOND_CHARACTER(_s)            _s[1]
      u32_t    ID_counter;
      task_t *taskhdl;

      enum {
              FILE_CONTENT_TASK_NAME,
              FILE_CONTENT_TASK_PRIO,
              FILE_CONTENT_TASK_FREESTACK,
              FILE_CONTENT_TASK_USEDMEM,
              FILE_CONTENT_TASK_OPENFILES,
              FILE_CONTENT_TASK_COUNT,
              FILE_CONTENT_NONE
      } file_content;
static stdret_t    procfs_closedir_freedd  (void *fs_handle, DIR *dir);
static stdret_t    procfs_closedir_generic (void *fs_handle, DIR *dir);
static dirent_t    procfs_readdir_root     (void *fs_handle, DIR *dir);
static dirent_t    procfs_readdir_taskname (void *fs_handle, DIR *dir);
static dirent_t    procfs_readdir_bin      (void *fs_handle, DIR *dir);
static dirent_t    procfs_readdir_taskid   (void *fs_handle, DIR *dir);
static dirent_t    procfs_readdir_taskid_n (void *fs_handle, DIR *dir);
static inline void mutex_force_lock        (mutex_t *mtx);
 * @param[out]          **fs_handle             file system allocated memory
 * @param[in ]           *src_path              file source path
API_FS_INIT(procfs, void **fs_handle, const char *src_path)
        struct procfs *procfs    = calloc(1, sizeof(struct procfs));
        list_t        *file_list = list_new();
        mutex_t       *mtx       = mutex_new(MUTEX_NORMAL);
        if (procfs && file_list && mtx) {
                procfs->file_list    = file_list;
                procfs->resource_mtx = mtx;
                procfs->ID_counter   = 0;
                *fs_handle = procfs;
                return STD_RET_OK;
        } else {
                if (file_list) {
                        list_delete(file_list);
                if (mtx) {
                        mutex_delete(mtx);
                }

                if (procfs) {
                        free(procfs);
 * @brief Release file system
 * @param[in ]          *fs_handle              file system allocated memory
API_FS_RELEASE(procfs, void *fs_handle)
        struct procfs *procfs = fs_handle;
        if (mutex_lock(procfs->resource_mtx, 100)) {
                if (list_get_item_count(procfs->file_list) != 0) {
                        mutex_unlock(procfs->resource_mtx);
                        errno = EBUSY;
                        return STD_RET_ERROR;
                }
                critical_section_begin();
                mutex_unlock(procfs->resource_mtx);
                mutex_delete(procfs->resource_mtx);
                list_delete(procfs->file_list);
                free(procfs);
                critical_section_end();
                return STD_RET_OK;
        } else {
                errno = EBUSY;
                return STD_RET_ERROR;
        }
 * @brief Open file
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[out]          *extra                  file extra data
 * @param[out]          *fd                     file descriptor
 * @param[out]          *fpos                   file position
 * @param[in]           *path                   file path
 * @param[in]            flags                  file open flags (see vfs.h)
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
API_FS_OPEN(procfs, void *fs_handle, void **extra, fd_t *fd, u64_t *lseek, const char *path, int flags)
        struct sysmoni_taskstat task_data;
        struct file_info       *file_info;
        if (flags != O_RDONLY) {
                errno = EROFS;
                path += strlen(DIR_TASKID_STR) + 2;
                path = sys_strtoi((char*)path, 16, (i32_t*)&taskHdl);
                if (sysm_get_task_stat(taskHdl, &task_data) != STD_RET_OK) {
                        errno = ENOENT;
                path = strrchr(path, '/');
                if (path == NULL) {
                        errno = ENOENT;
                file_info = calloc(1, sizeof(struct file_info));
                if (file_info == NULL) {
                file_info->taskhdl = taskHdl;

                if (strcmp((char*) path, FILE_TASK_NAME_STR) == 0) {
                        file_info->file_content = FILE_CONTENT_TASK_NAME;
                } else if (strcmp((char*) path, FILE_TASK_FREESTACK_STR) == 0) {
                        file_info->file_content = FILE_CONTENT_TASK_FREESTACK;
                } else if (strcmp((char*) path, FILE_TASK_OPENFILES_STR) == 0) {
                        file_info->file_content = FILE_CONTENT_TASK_OPENFILES;
                } else if (strcmp((char*) path, FILE_TASK_PRIO_STR) == 0) {
                        file_info->file_content = FILE_CONTENT_TASK_PRIO;
                } else if (strcmp((char*) path, FILE_TASK_USEDMEM_STR) == 0) {
                        file_info->file_content = FILE_CONTENT_TASK_USEDMEM;
                        free(file_info);
                        errno = ENOENT;
                mutex_force_lock(procmem->resource_mtx);
                if (list_add_item(procmem->file_list, procmem->ID_counter, file_info) == 0) {
                        mutex_unlock(procmem->resource_mtx);
                mutex_unlock(procmem->resource_mtx);
                free(file_info);
        } else if (strncmp(path, "/"DIR_TASKNAME_STR"/", strlen(DIR_TASKNAME_STR) + 2) == 0) {
                path += strlen(DIR_TASKNAME_STR) + 2;
                while (n-- && sysm_get_ntask_stat(i++, &task_data) == STD_RET_OK) {
                        if (strcmp(path, task_data.task_name) == 0) {
                                file_info = calloc(1, sizeof(struct file_info));
                                if (file_info == NULL) {
                                        return STD_RET_ERROR;
                                }
                                file_info->taskhdl      = task_data.task_handle;
                                file_info->file_content = FILE_CONTENT_NONE;
                                mutex_force_lock(procmem->resource_mtx);
                                if (list_add_item(procmem->file_list,
                                                  procmem->ID_counter, file_info) == 0) {
                                        *fd = procmem->ID_counter++;
                                        *lseek = 0;
                                        mutex_unlock(procmem->resource_mtx);
                                        return STD_RET_OK;
                                }
                                mutex_unlock(procmem->resource_mtx);
                                free(file_info);
                                return STD_RET_ERROR;
                errno = ENOENT;
                return STD_RET_ERROR;
        } else {
                errno = ENOENT;
                return STD_RET_ERROR;
        }
 * @brief Close file
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *extra                  file extra data
 * @param[in ]           fd                     file descriptor
 * @param[in ]           force                  force close
 * @param[in ]          *file_owner             task which opened file (valid if force is true)
API_FS_CLOSE(procfs, void *fs_handle, void *extra, fd_t fd, bool force, task_t *file_owner)
        UNUSED_ARG(force);
        UNUSED_ARG(file_owner);
                mutex_force_lock(procmem->resource_mtx);
                        mutex_unlock(procmem->resource_mtx);
                mutex_unlock(procmem->resource_mtx);
 * @brief Write data to the file
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *extra                  file extra data
 * @param[in ]           fd                     file descriptor
 * @param[in ]          *src                    data source
 * @param[in ]           count                  number of bytes to write
 * @param[in ]          *fpos                   position in file

 * @return number of written bytes, -1 if error
API_FS_WRITE(procfs, void *fs_handle,void *extra, fd_t fd, const u8_t *src, size_t count, u64_t *fpos)
        UNUSED_ARG(count);
        UNUSED_ARG(fpos);
        errno = EROFS;

        return -1;
 * @brief Read data from file
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *extra                  file extra data
 * @param[in ]           fd                     file descriptor
 * @param[out]          *dst                    data destination
 * @param[in ]           count                  number of bytes to read
 * @param[in ]          *fpos                   position in file

 * @return number of read bytes, -1 if error
API_FS_READ(procfs, void *fs_handle, void *extra, fd_t fd, u8_t *dst, size_t count, u64_t *fpos)
        STOP_IF(!count);
        STOP_IF(!fpos);
        mutex_force_lock(procmem->resource_mtx);
        struct file_info *file_info = list_get_iditem_data(procmem->file_list, fd);
        mutex_unlock(procmem->resource_mtx);
        if (file_info == NULL) {
                errno = ENOENT;
                return -1;
        if (file_info->file_content >= FILE_CONTENT_TASK_COUNT) {
                errno = ENOENT;
                return -1;
        struct sysmoni_taskstat task_info;
        if (sysm_get_task_stat(file_info->taskhdl, &task_info) != STD_RET_OK) {
                return -1;
        uint data_size = (CONFIG_RTOS_TASK_NAME_LEN > U32_STR_LEN) ? CONFIG_RTOS_TASK_NAME_LEN + 1 : U32_STR_LEN;
        char *data = calloc(data_size + 1, 1);
                return -1;
        switch (file_info->file_content) {
        case FILE_CONTENT_TASK_FREESTACK:
                data_size = sys_snprintf(data, data_size, "%u\n", task_info.free_stack);
        case FILE_CONTENT_TASK_NAME:
                data_size = sys_snprintf(data, data_size, "%s\n", task_info.task_name);
        case FILE_CONTENT_TASK_OPENFILES:
                data_size = sys_snprintf(data, data_size, "%u\n", task_info.opened_files);
        case FILE_CONTENT_TASK_PRIO:
                data_size = sys_snprintf(data, data_size, "%d\n", task_info.priority);
        case FILE_CONTENT_TASK_USEDMEM:
                data_size = sys_snprintf(data, data_size, "%u\n", task_info.memory_usage);
        default:
                free(data);
                return -1;
        size_t seek = *fpos > SIZE_MAX ? SIZE_MAX : *fpos;
        if (seek > data_size) {
                if (data_size - seek <= count) {
                        n = data_size - seek;
                        strncpy((char*)dst, data + seek, n);
                        n = count;
                        strncpy((char *)dst, data + seek, n);
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *extra                  file extra data
 * @param[in ]           fd                     file descriptor
 * @param[in ]           request                request
 * @param[in ][out]     *arg                    request's argument
API_FS_IOCTL(procfs, void *fs_handle, void *extra, fd_t fd, int request, void *arg)
        UNUSED_ARG(request);
        UNUSED_ARG(arg);

        errno = EPERM;
 * @brief Flush file data
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *extra                  file extra data
 * @param[in ]           fd                     file descriptor
API_FS_FLUSH(procfs, void *fs_handle, void *extra, fd_t fd)
        errno = EROFS;

 * @brief Return file status
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *extra                  file extra data
 * @param[in ]           fd                     file descriptor
 * @param[out]          *stat                   file status
API_FS_FSTAT(procfs, void *fs_handle, void *extra, fd_t fd, struct stat *stat)
        mutex_force_lock(procmem->resource_mtx);
        struct file_info *file_info = list_get_iditem_data(procmem->file_list, fd);
        mutex_unlock(procmem->resource_mtx);
        if (file_info == NULL) {
                errno = ENOENT;
        if (file_info->file_content >= FILE_CONTENT_TASK_COUNT) {
                errno = ENOENT;
        if (sysm_get_task_stat(file_info->taskhdl, &taskInfo) != STD_RET_OK) {
        stat->st_mode  = S_IRUSR | S_IRGRO | S_IROTH;
        stat->st_type  = FILE_TYPE_REGULAR;
        char data[U32_STR_LEN] = {0};
        switch (file_info->file_content) {
        case FILE_CONTENT_TASK_FREESTACK:
                stat->st_size = sys_snprintf(data, sizeof(data), "%u\n", taskInfo.free_stack);
        case FILE_CONTENT_TASK_NAME:
        case FILE_CONTENT_TASK_OPENFILES:
                stat->st_size = sys_snprintf(data, sizeof(data), "%u\n", taskInfo.opened_files);
        case FILE_CONTENT_TASK_PRIO:
                stat->st_size = sys_snprintf(data, sizeof(data), "%d\n", taskInfo.priority);
        case FILE_CONTENT_TASK_USEDMEM:
                stat->st_size = sys_snprintf(data, sizeof(data), "%u\n", taskInfo.memory_usage);
        default:
                return STD_RET_ERROR;
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   name of created directory
 * @param[in ]           mode                   dir mode
API_FS_MKDIR(procfs, void *fs_handle, const char *path, mode_t mode)
        UNUSED_ARG(mode);

        errno = EROFS;

        return STD_RET_ERROR;
}

//==============================================================================
/**
 * @brief Create pipe
 *
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   name of created pipe
 * @param[in ]           mode                   pipe mode
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//==============================================================================
API_FS_MKFIFO(procfs, void *fs_handle, const char *path, mode_t mode)
{
        STOP_IF(!fs_handle);
        STOP_IF(!path);
        UNUSED_ARG(mode);

        /* not supported by this file system */
        errno = EPERM;
 * @brief Create node for driver file
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   name of created node
 * @param[in ]          *drv_if                 driver interface
API_FS_MKNOD(procfs, void *fs_handle, const char *path, const struct vfs_drv_interface *drv_if)
        /* not supported by this file system */
        errno = EPERM;

 * @brief Open directory
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   name of opened directory
 * @param[in ]          *dir                    directory object
API_FS_OPENDIR(procfs, void *fs_handle, const char *path, DIR *dir)
                dir->f_items    = 3;
                dir->f_closedir = procfs_closedir_generic;
                dir->f_closedir = procfs_closedir_generic;
        } else if (strcmp(path, "/"DIR_BIN_STR"/") == 0) {
                dir->f_dd       = NULL;
                dir->f_items    = _get_programs_table_size();
                dir->f_readdir  = procfs_readdir_bin;
                dir->f_closedir = procfs_closedir_generic;
                return STD_RET_OK;
                path += strlen(DIR_TASKID_STR) + 2;

                i32_t taskval = 0;
                path = sys_strtoi((char*)path, 16, &taskval);

                if (FIRST_CHARACTER(path) == '/' && SECOND_CHARACTER(path) == '\0') {
                        struct sysmoni_taskstat taskdata;
                        task_t                 *taskHdl = (task_t *)taskval;
                        if (sysm_get_task_stat(taskHdl, &taskdata) == STD_RET_OK) {
                                dir->f_dd       = taskHdl;
                                dir->f_items    = FILE_CONTENT_TASK_COUNT;
                                dir->f_readdir  = procfs_readdir_taskid_n;
                                dir->f_closedir = procfs_closedir_generic;
                                return STD_RET_OK;
                        }
        errno = ENOENT;
static stdret_t procfs_closedir_freedd(void *fs_handle, DIR *dir)
        } else {
                return STD_RET_ERROR;
static stdret_t procfs_closedir_generic(void *fs_handle, DIR *dir)
 * @brief Remove file/directory
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   name of removed file/directory
API_FS_REMOVE(procfs, void *fs_handle, const char *path)
        errno = EROFS;

 * @brief Rename file/directory
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *old_name               old object name
 * @param[in ]          *new_name               new object name
API_FS_RENAME(procfs, void *fs_handle, const char *old_name, const char *new_name)
        errno = EROFS;

 * @brief Change file's mode
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   file path
 * @param[in ]           mode                   new file mode
API_FS_CHMOD(procfs, void *fs_handle, const char *path, int mode)
        errno = EROFS;

 * @brief Change file's owner and group
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   file path
 * @param[in ]           owner                  new file owner
 * @param[in ]           group                  new file group
API_FS_CHOWN(procfs, void *fs_handle, const char *path, int owner, int group)
        errno = EROFS;

 * @brief Return file/dir status
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   file path
 * @param[out]          *stat                   file status
API_FS_STAT(procfs, void *fs_handle, const char *path, struct stat *stat)
        stat->st_mode  = S_IRUSR | S_IRGRO | S_IROTH;
        stat->st_type  = FILE_TYPE_REGULAR;
 * @brief Return file system status
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[out]          *statfs                 file system status
API_FS_STATFS(procfs, void *fs_handle, struct vfs_statfs *statfs)
        statfs->f_fsname = "procfs";
static dirent_t procfs_readdir_root(void *fs_handle, DIR *dir)
        case  0: dirent.name = DIR_TASKID_STR;   break;
        case  1: dirent.name = DIR_TASKNAME_STR; break;
        case  2: dirent.name = DIR_BIN_STR;      break;
        default: break;
static dirent_t procfs_readdir_taskname(void *fs_handle, DIR *dir)
static dirent_t procfs_readdir_bin(void *fs_handle, DIR *dir)
{
        UNUSED_ARG(fs_handle);

        STOP_IF(!dir);

        dirent_t dirent;
        dirent.name = NULL;
        dirent.size = 0;

        if (dir->f_seek < (size_t)_get_programs_table_size()) {
                dirent.filetype = FILE_TYPE_PROGRAM;
                dirent.name     = _get_programs_table()[dir->f_seek].program_name;
                dirent.size     = 0;

                dir->f_seek++;
        }

        return dirent;
}

//==============================================================================
/**
 * @brief Read item from opened directory
 *
 * @param[in]  *fs_handle    file system data
 * @param[out] *dir          directory object
 *
 * @return directory entry
 */
//==============================================================================
static dirent_t procfs_readdir_taskid(void *fs_handle, DIR *dir)
                        sys_snprintf(dir->f_dd, TASK_ID_STR_LEN, "%x", (int)taskdata.task_handle);
static dirent_t procfs_readdir_taskid_n(void *fs_handle, DIR *dir)
        if (dir->f_seek >= FILE_CONTENT_TASK_COUNT) {
        case FILE_CONTENT_TASK_NAME:
                dirent.name = FILE_TASK_NAME_STR;
        case FILE_CONTENT_TASK_PRIO:
                dirent.name = FILE_TASK_PRIO_STR;
                dirent.size = sys_snprintf(data, ARRAY_SIZE(data), "%d\n", taskdata.priority);
        case FILE_CONTENT_TASK_FREESTACK:
                dirent.name = FILE_TASK_FREESTACK_STR;
                dirent.size = sys_snprintf(data, ARRAY_SIZE(data), "%u\n", taskdata.free_stack);
        case FILE_CONTENT_TASK_USEDMEM:
                dirent.name = FILE_TASK_USEDMEM_STR;
                dirent.size = sys_snprintf(data, ARRAY_SIZE(data), "%d\n", taskdata.memory_usage);
        case FILE_CONTENT_TASK_OPENFILES:
                dirent.name = FILE_TASK_OPENFILES_STR;
                dirent.size = sys_snprintf(data, ARRAY_SIZE(data), "%d\n", taskdata.opened_files);
//==============================================================================
/**
 * @brief Force lock mutex
 *
 * @param mtx           mutex
 */
//==============================================================================
static inline void mutex_force_lock(mutex_t *mtx)
{
        while (mutex_lock(mtx, MTX_BLOCK_TIME) != true);
}
