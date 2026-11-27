/* Local-only ownership markers cannot be supplied by received file contents. */
#ifdef _WIN32
int bsp_checkpoint_mark(FILE *file) {
    HANDLE handle = (HANDLE)_get_osfhandle(_fileno(file));
    BY_HANDLE_FILE_INFORMATION info;
    FILE_BASIC_INFO basic;
    if (handle == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(handle, &info))
        return 0;
    memset(&basic, 0, sizeof basic);
    basic.FileAttributes = (info.dwFileAttributes & ~FILE_ATTRIBUTE_NORMAL) | FILE_ATTRIBUTE_TEMPORARY;
    return SetFileInformationByHandle(handle, FileBasicInfo, &basic, sizeof basic) != 0;
}
int bsp_checkpoint_unmark(const char *path) {
    wchar_t wide[BS_PATH_MAX + 1];
    HANDLE handle;
    BY_HANDLE_FILE_INFORMATION info;
    FILE_BASIC_INFO basic;
    int okay = 0;
    if (!windows_path(path, wide))
        return 0;
    handle = CreateFileW(wide, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        handle=CreateFileW(wide,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
        if(handle==INVALID_HANDLE_VALUE)return 0;
        okay=GetFileInformationByHandle(handle,&info) && !(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_TEMPORARY));
        CloseHandle(handle);return okay;
    }
    if (GetFileInformationByHandle(handle, &info) &&
        !(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))) {
        okay = 1;
        if (info.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) {
            memset(&basic, 0, sizeof basic);
            basic.FileAttributes = info.dwFileAttributes & ~FILE_ATTRIBUTE_TEMPORARY;
            if (!basic.FileAttributes)
                basic.FileAttributes = FILE_ATTRIBUTE_NORMAL;
            okay = SetFileInformationByHandle(handle, FileBasicInfo, &basic, sizeof basic) != 0;
        }
    }
    CloseHandle(handle);
    return okay;
}
#else
#include <sys/xattr.h>
#ifdef __APPLE__
#define CHECKPOINT_ATTRIBUTE "org.bitskim.checkpoint"
#define CHECKPOINT_GET(fd, b, n) fgetxattr(fd, CHECKPOINT_ATTRIBUTE, b, n, 0, 0)
#define CHECKPOINT_SET(fd, b, n) fsetxattr(fd, CHECKPOINT_ATTRIBUTE, b, n, 0, 0)
#define CHECKPOINT_REMOVE(fd) fremovexattr(fd, CHECKPOINT_ATTRIBUTE, 0)
#else
#define CHECKPOINT_ATTRIBUTE "user.org.bitskim.checkpoint"
#define CHECKPOINT_GET(fd, b, n) fgetxattr(fd, CHECKPOINT_ATTRIBUTE, b, n)
#define CHECKPOINT_SET(fd, b, n) fsetxattr(fd, CHECKPOINT_ATTRIBUTE, b, n, 0)
#define CHECKPOINT_REMOVE(fd) fremovexattr(fd, CHECKPOINT_ATTRIBUTE)
#endif
static const char checkpoint_tag[] = "BitSkim partial v1";
static int retained_owned(int fd) {
    char value[sizeof checkpoint_tag];
    return CHECKPOINT_GET(fd, value, sizeof value) == sizeof value &&
           !memcmp(value, checkpoint_tag, sizeof value);
}
int bsp_checkpoint_mark(FILE *file) {
    return CHECKPOINT_SET(fileno(file), checkpoint_tag, sizeof checkpoint_tag) == 0;
}
int bsp_checkpoint_unmark(const char *path) {
    int file, okay = 1;
    file = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (file < 0)
        return 0;
    if (retained_owned(file))
        okay = CHECKPOINT_REMOVE(file) == 0;
    close(file);
    return okay;
}
#endif
/* Cleanup acts only on checked regular checkpoint pairs, never final files. */
#ifdef _WIN32
static HANDLE retained_open(const char *root, const char *relative, const char *suffix, int remove,
                            BSFileStamp *stamp) {
    char path[BS_PATH_MAX + 1];
    wchar_t wide[BS_PATH_MAX + 1], base[BS_PATH_MAX + 1], actual[BS_PATH_MAX + 1];
    HANDLE dir = INVALID_HANDLE_VALUE, file = INVALID_HANDLE_VALUE;
    BY_HANDLE_FILE_INFORMATION info, rootinfo;
    DWORD n, length;
    size_t size;
    memset(stamp, 0, sizeof *stamp);
    if (strlen(root) + strlen(relative) + strlen(suffix) + 2 > sizeof path)
        return INVALID_HANDLE_VALUE;
    if (!windows_path(root, wide))
        return INVALID_HANDLE_VALUE;
    dir = CreateFileW(wide, FILE_READ_ATTRIBUTES,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (dir == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;
    if (!GetFileInformationByHandle(dir, &rootinfo) ||
        !(rootinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (rootinfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        goto done;
    n = GetFinalPathNameByHandleW(dir, base, BS_PATH_MAX + 1, FILE_NAME_NORMALIZED);
    if (!n || n > BS_PATH_MAX - 2)
        goto done;
    size = wcslen(base);
    if (base[size - 1] != L'\\') {
        base[size++] = L'\\';
        base[size] = 0;
    }
    sprintf(path, "%s/%s%s", root, relative, suffix);
    if (!windows_path(path, wide))
        goto done;
    file = CreateFileW(wide, GENERIC_READ | (remove ? DELETE : 0), FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE)
        goto done;
    length = GetFinalPathNameByHandleW(file, actual, BS_PATH_MAX + 1, FILE_NAME_NORMALIZED);
    if (!length || length > BS_PATH_MAX || _wcsnicmp(base, actual, size) ||
        !GetFileInformationByHandle(file, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        CloseHandle(file);
        file = INVALID_HANDLE_VALUE;
        goto done;
    }
    stamp->word[0] = info.dwVolumeSerialNumber;
    stamp->word[1] = info.nFileIndexHigh;
    stamp->word[2] = info.nFileIndexLow;
    stamp->word[3] = info.nFileSizeHigh;
    stamp->word[4] = info.nFileSizeLow;
    stamp->word[5] = info.ftLastWriteTime.dwHighDateTime;
    stamp->word[6] = info.ftLastWriteTime.dwLowDateTime;
    stamp->word[7] = info.dwFileAttributes;
    stamp->word[8] = info.ftCreationTime.dwHighDateTime;
    stamp->word[9] = info.ftCreationTime.dwLowDateTime;
    stamp->word[10] = rootinfo.dwVolumeSerialNumber;
    stamp->word[11] = rootinfo.nFileIndexHigh;
    stamp->word[12] = rootinfo.nFileIndexLow;
    {
        FILE_STANDARD_INFO standard;
        if (GetFileInformationByHandleEx(file, FileStandardInfo, &standard, sizeof standard)) {
            stamp->word[13] = standard.AllocationSize.HighPart;
            stamp->word[14] = standard.AllocationSize.LowPart;
        } else {
            stamp->word[13] = stamp->word[3];
            stamp->word[14] = stamp->word[4];
        }
    }
done:
    CloseHandle(dir);
    return file;
}
int bsp_retained_pair(const char *root, const char *relative, unsigned char *header, int *length,
                      unsigned long *bytes, BSFileStamp *part, BSFileStamp *meta) {
    HANDLE p, m;
    DWORD got;
    int okay = 0;
    p = retained_open(root, relative, ".bsk-part", 0, part);
    if (p == INVALID_HANDLE_VALUE)
        return 0;
    m = retained_open(root, relative, ".bsk-resume", 0, meta);
    if (m == INVALID_HANDLE_VALUE) {
        CloseHandle(p);
        return 0;
    }
    if ((part->word[7] & FILE_ATTRIBUTE_TEMPORARY) && !part->word[3] && !meta->word[3] &&
        meta->word[4] >= 47 && meta->word[4] <= 301 &&
        ReadFile(m, header, meta->word[4], &got, NULL) && got == meta->word[4]) {
        *length = (int)got;
        *bytes = part->word[4];
        okay = 1;
    }
    CloseHandle(m);
    CloseHandle(p);
    return okay;
}
int bsp_retained_unlink(const char *root, const char *relative, const BSFileStamp *part,
                        const BSFileStamp *meta) {
    BSFileStamp ps, ms;
    HANDLE p, m;
    FILE_DISPOSITION_INFO deletion;
    int okay = 0;
    p = retained_open(root, relative, ".bsk-part", 1, &ps);
    if (p == INVALID_HANDLE_VALUE)
        return 0;
    m = retained_open(root, relative, ".bsk-resume", 1, &ms);
    if (m == INVALID_HANDLE_VALUE) {
        CloseHandle(p);
        return 0;
    }
    if (!memcmp(part, &ps, sizeof ps) && !memcmp(meta, &ms, sizeof ms)) {
        deletion.DeleteFile = TRUE;
        okay = SetFileInformationByHandle(p, FileDispositionInfo, &deletion, sizeof deletion) &&
               SetFileInformationByHandle(m, FileDispositionInfo, &deletion, sizeof deletion);
        if (!okay) {
            deletion.DeleteFile = FALSE;
            SetFileInformationByHandle(p, FileDispositionInfo, &deletion, sizeof deletion);
            SetFileInformationByHandle(m, FileDispositionInfo, &deletion, sizeof deletion);
        }
    }
    CloseHandle(m);
    CloseHandle(p);
    return okay;
}
#else
#include <sys/file.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
static int retained_parent(const char *root, const char *relative, char *leaf,
                           struct stat *rootinfo) {
    char path[BS_PATH_MAX + 1], *part, *slash;
    int fd, next;
    if (!root || !relative || !*relative || relative[0] == '/' || strlen(relative) > BS_PATH_MAX)
        return -1;
    fd = open(root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (fd < 0)
        return -1;
    if (fstat(fd, rootinfo)) {
        close(fd);
        return -1;
    }
    strcpy(path, relative);
    part = path;
    for (;;) {
        slash = strchr(part, '/');
        if (slash)
            *slash = 0;
        if (!*part || !strcmp(part, ".") || !strcmp(part, "..")) {
            close(fd);
            return -1;
        }
        if (!slash) {
            strcpy(leaf, part);
            return fd;
        }
        next = openat(fd, part, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
        close(fd);
        if (next < 0)
            return -1;
        fd = next;
        part = slash + 1;
    }
}
static int retained_stamp(int fd, const struct stat *root, BSFileStamp *stamp) {
    struct stat info;
    memset(stamp, 0, sizeof *stamp);
    if (fstat(fd, &info) || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        (unsigned long)info.st_size > BS_FILE_MAX)
        return 0;
    stamp->word[0] = (unsigned long)info.st_dev;
    stamp->word[1] = (unsigned long)info.st_ino;
    stamp->word[2] = (unsigned long)info.st_size;
    stamp->word[3] = (unsigned long)info.st_mtime;
    stamp->word[5] = (unsigned long)info.st_mode;
    stamp->word[6] = (unsigned long)info.st_ctime;
#ifdef __APPLE__
    stamp->word[4] = (unsigned long)info.st_mtimespec.tv_nsec;
    stamp->word[7] = (unsigned long)info.st_ctimespec.tv_nsec;
#else
    stamp->word[4] = (unsigned long)info.st_mtim.tv_nsec;
    stamp->word[7] = (unsigned long)info.st_ctim.tv_nsec;
#endif
    stamp->word[10] = (unsigned long)root->st_dev;
    stamp->word[11] = (unsigned long)root->st_ino;
    fs_product((unsigned long)info.st_blocks, 512UL, &stamp->word[13], &stamp->word[14]);
    return 1;
}
static int retained_files(const char *root, const char *relative, int *parent, int *part, int *meta,
                          char *pname, char *mname, struct stat *info, BSFileStamp *ps,
                          BSFileStamp *ms) {
    char leaf[BS_PATH_MAX + 1];
    *part = *meta = -1;
    *parent = retained_parent(root, relative, leaf, info);
    if (*parent < 0)
        return 0;
    if (strlen(leaf) > BS_PATH_MAX - 12)
        goto bad;
    sprintf(pname, "%s.bsk-part", leaf);
    sprintf(mname, "%s.bsk-resume", leaf);
    *part = openat(*parent, pname, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    *meta = openat(*parent, mname, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (*part >= 0 && *meta >= 0 && !flock(*part, LOCK_EX | LOCK_NB) &&
        !flock(*meta, LOCK_EX | LOCK_NB) && retained_stamp(*part, info, ps) &&
        retained_stamp(*meta, info, ms) && retained_owned(*part))
        return 1;
bad:
    if (*part >= 0)
        close(*part);
    if (*meta >= 0)
        close(*meta);
    close(*parent);
    return 0;
}
int bsp_retained_pair(const char *root, const char *relative, unsigned char *header, int *length,
                      unsigned long *bytes, BSFileStamp *part, BSFileStamp *meta) {
    struct stat info;
    int dir, p, m, okay = 0;
    char pn[BS_PATH_MAX + 1], mn[BS_PATH_MAX + 1];
    if (!retained_files(root, relative, &dir, &p, &m, pn, mn, &info, part, meta))
        return 0;
    if (meta->word[2] >= 47 && meta->word[2] <= 301 &&
        read(m, header, (size_t)meta->word[2]) == (long)meta->word[2]) {
        *length = (int)meta->word[2];
        *bytes = part->word[2];
        okay = 1;
    }
    close(m);
    close(p);
    close(dir);
    return okay;
}
static int retained_move(int dir, const char *from, const char *to) {
#ifdef __APPLE__
    return renameatx_np(dir, from, dir, to, RENAME_EXCL);
#elif defined(SYS_renameat2)
    return (int)syscall(SYS_renameat2, dir, from, dir, to, 1);
#else
    (void)dir;
    (void)from;
    (void)to;
    return -1;
#endif
}
static int retained_same(int dir, const char *name, const struct stat *root,
                         const BSFileStamp *stamp) {
    int file;
    BSFileStamp now;
    file = openat(dir, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (file < 0)
        return 0;
    {
        int same = retained_stamp(file, root, &now) &&
                   !memcmp(now.word, stamp->word, 6 * sizeof(unsigned long));
        close(file);
        return same;
    }
}
int bsp_retained_unlink(const char *root, const char *relative, const BSFileStamp *part,
                        const BSFileStamp *meta) {
    struct stat info;
    BSFileStamp ps, ms;
    int dir, p, m, okay = 0, movedp = 0, movedm = 0, i;
    unsigned char random[12];
    char pn[BS_PATH_MAX + 1], mn[BS_PATH_MAX + 1], pq[48], mq[48];
    static const char hex[] = "0123456789abcdef";
    if (!retained_files(root, relative, &dir, &p, &m, pn, mn, &info, &ps, &ms))
        return 0;
    if (memcmp(part, &ps, sizeof ps) || memcmp(meta, &ms, sizeof ms) ||
        RAND_bytes(random, sizeof random) != 1)
        goto done;
    strcpy(pq, ".bsk-cleanup-");
    for (i = 0; i < 12; i++) {
        pq[12 + i * 2] = hex[random[i] >> 4];
        pq[13 + i * 2] = hex[random[i] & 15];
    }
    pq[36] = 0;
    strcpy(mq, pq);
    strcat(pq, "-p");
    strcat(mq, "-m");
    if (retained_move(dir, pn, pq))
        goto done;
    movedp = 1;
    if (retained_move(dir, mn, mq))
        goto restore;
    movedm = 1;
    if (!retained_same(dir, pq, &info, part) || !retained_same(dir, mq, &info, meta))
        goto restore;
    if (unlinkat(dir, pq, 0))
        goto restore;
    movedp = 0;
    if (unlinkat(dir, mq, 0)) {
        retained_move(dir, mq, mn);
        okay = -1;
        goto done;
    }
    okay = 1;
    goto done;
restore:
    if (movedm)
        retained_move(dir, mq, mn);
    if (movedp)
        retained_move(dir, pq, pn);
done:
    close(m);
    close(p);
    close(dir);
    return okay;
}
#endif
