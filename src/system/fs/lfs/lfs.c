#include "system/dnxfs.h"
#include "system/thread.h"
#define MTX_BLOCK_TIME                  10
#define PIPE_LENGTH                     CONFIG_STREAM_BUFFER_LENGTH
#define PIPE_WRITE_TIMEOUT              MAX_DELAY
#define PIPE_READ_TIMEOUT               MAX_DELAY
        NODE_TYPE_LINK = FILE_TYPE_LINK,
        NODE_TYPE_PIPE = FILE_TYPE_PIPE
        mode_t           mode;                  /* protection                */
static inline char      get_first_char                  (const char *str);
static inline char      get_last_char                   (const char *str);
static void             mutex_force_lock                (mutex_t *mtx);
static node_t          *new_node                        (struct LFS_data *lfs, node_t *nodebase, char *filename, i32_t *item);
static stdret_t         delete_node                     (node_t *base, node_t *target, u32_t baseitemid);
static node_t          *get_node                        (const char *path, node_t *startnode, i32_t deep, i32_t *item);
static uint             get_path_deep                   (const char *path);
static dirent_t         lfs_readdir                     (void *fs_handle, DIR *dir);
static stdret_t         lfs_closedir                    (void *fs_handle, DIR *dir);
static stdret_t         add_node_to_list_of_open_files  (struct LFS_data *lfs, node_t *base_node, node_t *node, i32_t *item);
 * @param[out]          **fs_handle             file system allocated memory
 * @param[in ]           *src_path              file source path
API_FS_INIT(lfs, void **fs_handle, const char *src_path)
        lfs->resource_mtx  = mutex_new(MUTEX_RECURSIVE);
        lfs->root_dir.data = list_new();
        lfs->list_of_opended_files = list_new();
                        mutex_delete(lfs->resource_mtx);
                        list_delete(lfs->root_dir.data);
                        list_delete(lfs->list_of_opended_files);
 * @brief Release file system
 * @param[in ]          *fs_handle              file system allocated memory
API_FS_RELEASE(lfs, void *fs_handle)
        errno = EPERM;

 * @brief Create node for driver file
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   name of created node
 * @param[in ]          *drv_if                 driver interface
API_FS_MKNOD(lfs, void *fs_handle, const char *path, const struct vfs_drv_interface *drv_if)
        mutex_force_lock(lfs->resource_mtx);
                                mutex_unlock(lfs->resource_mtx);
        mutex_unlock(lfs->resource_mtx);
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   name of created directory
 * @param[in ]           mode                   dir mode
API_FS_MKDIR(lfs, void *fs_handle, const char *path, mode_t mode)
        mutex_force_lock(lfs->resource_mtx);
        if (base_node == NULL) {
                goto error;
        }

        if (file_node != NULL) {
                errno = EEXIST;
                if ((new_dir->data = list_new())) {
                        new_dir->mode = mode;
                                mutex_unlock(lfs->resource_mtx);
                                list_delete(new_dir->data);
        mutex_unlock(lfs->resource_mtx);
 * @brief Create pipe
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   name of created pipe
 * @param[in ]           mode                   pipe mode
API_FS_MKFIFO(lfs, void *fs_handle, const char *path, mode_t mode)
{
        STOP_IF(!fs_handle);
        STOP_IF(!path);

        struct LFS_data *lfs = fs_handle;

        if (FIRST_CHARACTER(path) != '/') {
                errno = ENOENT;
                return STD_RET_ERROR;
        }

        mutex_force_lock(lfs->resource_mtx);
        node_t *dir_node  = get_node(path, &lfs->root_dir, -1, NULL);
        node_t *fifo_node = get_node(strrchr(path, '/'), dir_node, 0, NULL);

        /* directory must exist and driver's file not */
        if (!dir_node) {
                goto error;
        }

        if (fifo_node) {
                errno = EEXIST;
                goto error;
        }

        if (dir_node->type != NODE_TYPE_DIR) {
                goto error;
        }

        char *fifo_name     = strrchr(path, '/') + 1;
        uint  fifo_name_len = strlen(fifo_name);

        char *fifo_file_name = calloc(fifo_name_len + 1, sizeof(char));
        if (fifo_file_name) {
                strcpy(fifo_file_name, fifo_name);

                node_t  *fifo_file  = calloc(1, sizeof(node_t));
                queue_t *fifo_queue = queue_new(PIPE_LENGTH, sizeof(u8_t));

                if (fifo_file && fifo_queue) {

                        fifo_file->name = fifo_file_name;
                        fifo_file->size = 0;
                        fifo_file->type = NODE_TYPE_PIPE;
                        fifo_file->data = fifo_queue;
                        fifo_file->fd   = 0;
                        fifo_file->mode = mode;

                        /* add pipe to folder */
                        if (list_add_item(dir_node->data, lfs->id_counter++, fifo_file) >= 0) {
                                mutex_unlock(lfs->resource_mtx);
                                return STD_RET_OK;
                        }
                }

                /* free memory when error */
                if (fifo_file) {
                        free(fifo_file);
                }

                if (fifo_queue) {
                        queue_delete(fifo_queue);
                }

                free(fifo_file_name);
        }

error:
        mutex_unlock(lfs->resource_mtx);
        return STD_RET_ERROR;
}

//==============================================================================
/**
 * @brief Open directory
 *
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   name of opened directory
 * @param[in ]          *dir                    directory object
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//==============================================================================
API_FS_OPENDIR(lfs, void *fs_handle, const char *path, DIR *dir)
        mutex_force_lock(lfs->resource_mtx);
                        mutex_unlock(lfs->resource_mtx);
        mutex_unlock(lfs->resource_mtx);
static stdret_t lfs_closedir(void *fs_handle, DIR *dir)
static dirent_t lfs_readdir(void *fs_handle, DIR *dir)
        mutex_force_lock(lfs->resource_mtx);
                        struct vfs_dev_stat dev_stat;
                        dev_stat.st_size = 0;
                        drv_if->drv_stat(drv_if->handle, &dev_stat);
                        node->size = dev_stat.st_size;
        mutex_unlock(lfs->resource_mtx);
 * @brief Remove file/directory
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   name of removed file/directory
API_FS_REMOVE(lfs, void *fs_handle, const char *path)
        mutex_force_lock(lfs->resource_mtx);
        bool    remove_file = true;
        if (get_last_char(path) == '/') {
                        errno = ENOTDIR;
                                opened_file->remove_at_close = true;
                                remove_file = false;
        if (remove_file == true) {
                        mutex_unlock(lfs->resource_mtx);
                } else {
                        errno = ENOENT;
                mutex_unlock(lfs->resource_mtx);
        mutex_unlock(lfs->resource_mtx);
 * @brief Rename file/directory
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *old_name               old object name
 * @param[in ]          *new_name               new object name
API_FS_RENAME(lfs, void *fs_handle, const char *old_name, const char *new_name)
        mutex_force_lock(lfs->resource_mtx);
        if (get_first_char(old_name) != '/' || get_first_char(new_name) != '/') {
        if (get_last_char(old_name) == '/' || get_last_char(new_name) == '/') {
                mutex_unlock(lfs->resource_mtx);
        mutex_unlock(lfs->resource_mtx);
 * @brief Change file's mode
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   file path
 * @param[in ]           mode                   new file mode
API_FS_CHMOD(lfs, void *fs_handle, const char *path, int mode)
        mutex_force_lock(lfs->resource_mtx);
                mutex_unlock(lfs->resource_mtx);
        mutex_unlock(lfs->resource_mtx);
 * @brief Change file's owner and group
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   file path
 * @param[in ]           owner                  new file owner
 * @param[in ]           group                  new file group
API_FS_CHOWN(lfs, void *fs_handle, const char *path, int owner, int group)
        mutex_force_lock(lfs->resource_mtx);
                mutex_unlock(lfs->resource_mtx);
        mutex_unlock(lfs->resource_mtx);
 * @brief Return file/dir status
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *path                   file path
 * @param[out]          *stat                   file status
API_FS_STAT(lfs, void *fs_handle, const char *path, struct stat *stat)
        mutex_force_lock(lfs->resource_mtx);
                if ( (get_last_char(path) == '/' && node->type == NODE_TYPE_DIR)
                   || get_last_char(path) != '/') {
                                struct vfs_dev_stat dev_stat;
                                dev_stat.st_size = 0;
                                drv_if->drv_stat(drv_if->handle, &dev_stat);
                                node->size   = dev_stat.st_size;
                                stat->st_dev = dev_stat.st_major;
                        } else {
                                stat->st_dev = node->fd;
                        stat->st_type  = node->type;
                        mutex_unlock(lfs->resource_mtx);
        mutex_unlock(lfs->resource_mtx);
 * @brief Return file status
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *extra                  file extra data
 * @param[in ]           fd                     file descriptor
 * @param[out]          *stat                   file status
API_FS_FSTAT(lfs, void *fs_handle, void *extra, fd_t fd, struct stat *stat)
        mutex_force_lock(lfs->resource_mtx);
                                struct vfs_dev_stat dev_stat;
                                dev_stat.st_size = 0;
                                drv_if->drv_stat(drv_if->handle, &dev_stat);
                                opened_file->node->size = dev_stat.st_size;
                                stat->st_dev = dev_stat.st_major;
                        } else {
                                stat->st_dev = opened_file->node->fd;
                        stat->st_type  = opened_file->node->type;
                        mutex_unlock(lfs->resource_mtx);
        errno = ENOENT;

        mutex_unlock(lfs->resource_mtx);
 * @brief Return file system status
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[out]          *statfs                 file system status
API_FS_STATFS(lfs, void *fs_handle, struct vfs_statfs *statfs)
        statfs->f_fsname = "lfs";
 * @brief Open file
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[out]          *extra                  file extra data
 * @param[out]          *fd                     file descriptor
 * @param[out]          *fpos                   file position
 * @param[in]           *path                   file path
 * @param[in]            flags                  file open flags (see vfs.h)
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
API_FS_OPEN(lfs, void *fs_handle, void **extra, fd_t *fd, u64_t *fpos, const char *path, int flags)
        STOP_IF(!fpos);
        mutex_force_lock(lfs->resource_mtx);
                if (!(flags & O_CREAT)) {
                if ((flags & O_CREAT) && !(flags & O_APPEND)) {
                if (!(flags & O_APPEND)) {
                        *fpos = 0;
                } else {
                        *fpos = node->size;
                if (drv->drv_open(drv->handle, O_DEV_FLAGS(flags)) == STD_RET_OK) {
                        *fpos = 0;
        mutex_unlock(lfs->resource_mtx);
        errno = ENOENT;
        mutex_unlock(lfs->resource_mtx);
 * @brief Close file
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *extra                  file extra data
 * @param[in ]           fd                     file descriptor
 * @param[in ]           force                  force close
 * @param[in ]          *file_owner             task which opened file (valid if force is true)
API_FS_CLOSE(lfs, void *fs_handle, void *extra, fd_t fd, bool force, const task_t *file_owner)
        mutex_force_lock(lfs->resource_mtx);
                if ((status = drv_if->drv_close(drv_if->handle, force, file_owner)) != STD_RET_OK) {
        if (opened_file_data.remove_at_close == true) {
exit:
        errno = ENOENT;

        mutex_unlock(lfs->resource_mtx);
 * @brief Write data to the file
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *extra                  file extra data
 * @param[in ]           fd                     file descriptor
 * @param[in ]          *src                    data source
 * @param[in ]           count                  number of bytes to write
 * @param[in ]          *fpos                   position in file

 * @return number of written bytes, -1 if error
API_FS_WRITE(lfs, void *fs_handle, void *extra, fd_t fd, const u8_t *src, size_t count, u64_t *fpos)
        STOP_IF(!fpos);
        STOP_IF(!count);
        ssize_t          n   = -1;
        mutex_force_lock(lfs->resource_mtx);
                errno = ENOENT;
                errno = ENOENT;
                        mutex_unlock(lfs->resource_mtx);
                        return drv_if->drv_write(drv_if->handle, src, count, fpos);
                size_t write_size  = count;
                size_t seek        = *fpos > SIZE_MAX ? SIZE_MAX : *fpos;
                                errno = ENOSPC;
                        n = count;
                        n = count;
        } else if (node->type == NODE_TYPE_PIPE) {
                mutex_unlock(lfs->resource_mtx);
                if (node->data) {
                        n = 0;
                        for (uint i = 0; i < count; i++) {
                                if (queue_send(node->data, src + i, PIPE_WRITE_TIMEOUT)) {
                                        n++;
                                } else {
                                        i--;
                                }
                        }

                        critical_section_begin();
                        node->size += n;
                        critical_section_end();
                        return n;
                } else {
                        errno = EIO;
                }
        } else {
                errno = ENOENT;
        mutex_unlock(lfs->resource_mtx);
 * @brief Read data from file
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *extra                  file extra data
 * @param[in ]           fd                     file descriptor
 * @param[out]          *dst                    data destination
 * @param[in ]           count                  number of bytes to read
 * @param[in ]          *fpos                   position in file

 * @return number of read bytes, -1 if error
API_FS_READ(lfs, void *fs_handle, void *extra, fd_t fd, u8_t *dst, size_t count, u64_t *fpos)
        STOP_IF(!fpos);
        STOP_IF(!count);
        ssize_t          n   = -1;
        mutex_force_lock(lfs->resource_mtx);
                        mutex_unlock(lfs->resource_mtx);
                        return drv_if->drv_read(drv_if->handle, dst, count, fpos);
                size_t seek        = *fpos > SIZE_MAX ? SIZE_MAX : *fpos;
                if ((file_length - seek) >= count) {
                        items_to_read = count;
                        items_to_read = file_length - seek;
                                memcpy(dst, node->data + seek, items_to_read);
                        } else {
                                n = 0;
                } else {
                        errno = EIO;
        } else if (node->type == NODE_TYPE_PIPE) {
                mutex_unlock(lfs->resource_mtx);
                if (node->data) {
                        n = 0;
                        for (uint i = 0; i < count; i++) {
                                if (queue_receive(node->data, dst + i, PIPE_READ_TIMEOUT)) {
                                        n++;
                                }
                        }

                        critical_section_begin();
                        node->size -= n;
                        critical_section_end();
                        return n;
                } else {
                        errno = EIO;
                }
        } else {
                errno = EIO;
        mutex_unlock(lfs->resource_mtx);
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *extra                  file extra data
 * @param[in ]           fd                     file descriptor
 * @param[in ]           request                request
 * @param[in ][out]     *arg                    request's argument
API_FS_IOCTL(lfs, void *fs_handle, void *extra, fd_t fd, int request, void *arg)
        mutex_force_lock(lfs->resource_mtx);
                        mutex_unlock(lfs->resource_mtx);
                        return drv_if->drv_ioctl(drv_if->handle, request, arg);
        errno = ENOENT;
        mutex_unlock(lfs->resource_mtx);
 * @brief Flush file data
 * @param[in ]          *fs_handle              file system allocated memory
 * @param[in ]          *extra                  file extra data
 * @param[in ]           fd                     file descriptor
API_FS_FLUSH(lfs, void *fs_handle, void *extra, fd_t fd)
        mutex_force_lock(lfs->resource_mtx);
                        mutex_unlock(lfs->resource_mtx);
        errno = ENOENT;
        mutex_unlock(lfs->resource_mtx);
//==============================================================================
/**
 * @brief Return last character of selected string
 */
//==============================================================================
static inline char get_last_char(const char *str)
{
        return LAST_CHARACTER(str);
}

//==============================================================================
/**
 * @brief Return first character of selected string
 */
//==============================================================================
static inline char get_first_char(const char *str)
{
        return FIRST_CHARACTER(str);
}

//==============================================================================
/**
 * @brief Function force lock mutex
 *
 * @param mtx           mutex
 */
//==============================================================================
static void mutex_force_lock(mutex_t *mtx)
{
        while (mutex_lock(mtx, MTX_BLOCK_TIME) != true);
}

                        list_delete(target->data);
                        target->data = NULL;
                }
        } else if (target->type == NODE_TYPE_PIPE) {

                if (target->data) {
                        queue_delete(target->data);
 *        ERRNO: ENOENT
 * @param[in]  path             path
 * @param[in]  startnode        start node
 * @param[out] extPath          external path begin (pointer from path)
 * @param[in]  deep             deep control
 * @param[out] item             node is n-item of list which was found
                errno = ENOENT;
                errno = ENOENT;
                        errno        = ENOENT;
 * ERRNO: ENOENT, ENOTDIR, ENOMEM
                errno = ENOENT;
                errno = ENOTDIR;
                errno = ENOENT;
 *        ERRNO: ENOMEM
        opened_file_info->remove_at_close = false;
        opened_file_info->node            = node;
        opened_file_info->base_node       = base_node;
                if (opened_file->remove_at_close == true) {
                        opened_file_info->remove_at_close = true;
        errno = ENOMEM;