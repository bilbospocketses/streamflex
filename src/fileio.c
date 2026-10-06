// On Linux, for O_PATH: fileio_replace() reads the old file's permissions through a descriptor
// that needs no permission to read the file
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "fileio.h"
#include "alloc.h"
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <io.h>
#include <share.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <mntent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

// Each thread keeps its own reason, so a failure on one thread never changes what another reads
#ifdef _MSC_VER
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL _Thread_local
#endif

static THREAD_LOCAL char last_error[160] = "";
static THREAD_LOCAL char last_warning[160] = "";

// A function to remember why the last call failed
static void set_error(const char *reason)
{
    snprintf(last_error, sizeof(last_error), "%s", reason);
}

// A fault a unit test asks for, to reach a failure no real file can be made to give: which one,
// after how many entries (FILEIO_FAULT_LIST_READ), and with which error code
static FileioFault fault = FILEIO_FAULT_NONE;
static int fault_after = 0;
static int fault_code = 0;

// A function for the unit tests to make one kind of step fail; FILEIO_FAULT_NONE for none
void fileio_set_fault(FileioFault kind, int after, int code)
{
    fault = kind;
    fault_after = after;
    fault_code = code;
}

// A function to tell the caller why the last call failed
const char *fileio_last_error(void)
{
    return last_error;
}

// A function to tell the caller what the last replace could not do, though it succeeded; "" if nothing
const char *fileio_last_warning(void)
{
    return last_warning;
}

// A function to tell a path separator, in either style
bool fileio_is_separator(char c)
{
    return c == '/' || c == '\\';
}

// A function to lower-case an ASCII letter, for comparing names and extensions
int fileio_lower(char c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : (unsigned char) c;
}

// A function to find the last name in a path, ignoring a trailing separator
void fileio_base_name(const char *path, char *out, size_t size)
{
    size_t length = strlen(path);
    while (length > 1 && fileio_is_separator(path[length - 1]))
        length--;
    size_t start = length;
    while (start > 0 && !fileio_is_separator(path[start - 1]))
        start--;
    snprintf(out, size, "%.*s", (int) (length - start), path + start);
}

// A function to describe a C library error in a few words
static void set_errno_error(int code)
{
    switch (code) {
        case EACCES:
        case EPERM:
            set_error("permission denied");
            break;
#ifdef EROFS
        case EROFS:
            set_error("the file system is read-only");
            break;
#endif
        case ENOSPC:
            set_error("the disk is full");
            break;
        case ENOENT:
        case ENOTDIR:
            set_error("not found");
            break;
        case ENOMEM:
            set_error("out of memory");
            break;
        default: {
            // The system's own words, from the forms that write into a buffer: strerror() may share one
            // buffer between threads
            char buffer[128] = "";
            const char *text = buffer;
#ifdef _WIN32
            if (strerror_s(buffer, sizeof(buffer), code) != 0)
                buffer[0] = '\0';
#elif defined(__GLIBC__) && defined(_GNU_SOURCE)
            text = strerror_r(code, buffer, sizeof(buffer));
#else
            if (strerror_r(code, buffer, sizeof(buffer)) != 0)
                buffer[0] = '\0';
#endif
            if (text == NULL || text[0] == '\0')
                snprintf(last_error, sizeof(last_error), "error %d", code);
            else
                set_error(text);
        }
    }
}

#ifdef _WIN32
// A function to describe a Windows error code in a few words
static void set_windows_error(DWORD code)
{
    char text[64];
    switch (code) {
        case ERROR_ACCESS_DENIED:
            set_error("permission denied");
            break;
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            set_error("the file is in use by another program");
            break;
        case ERROR_DISK_FULL:
        case ERROR_HANDLE_DISK_FULL:
            set_error("the disk is full");
            break;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            set_error("not found");
            break;
        case ERROR_DIRECTORY:
            set_error("not a folder");
            break;
        case ERROR_WRITE_PROTECT:
            set_error("the disk is write-protected");
            break;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            set_error("out of memory");
            break;
        case ERROR_NETNAME_DELETED:
        case ERROR_UNEXP_NET_ERR:
        case ERROR_BAD_NETPATH:
        case ERROR_NETWORK_UNREACHABLE:
            set_error("the network share is not available");
            break;
        default:
            snprintf(text, sizeof(text), "Windows error %lu", (unsigned long) code);
            set_error(text);
    }
}

// A function to convert a UTF-8 string to a new UTF-16 one
static wchar_t *to_wide(const char *text)
{
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (count <= 0) {
        set_error("the path is not valid UTF-8");
        return NULL;
    }
    wchar_t *wide = alloc_malloc((size_t) count * sizeof(wchar_t));
    if (wide == NULL) {
        set_error("out of memory");
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, count);
    return wide;
}

// A function to convert a UTF-16 string to a new UTF-8 one
static char *to_utf8(const wchar_t *wide)
{
    int count = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (count <= 0) {
        set_windows_error(GetLastError());
        return NULL;
    }
    char *text = alloc_malloc((size_t) count);
    if (text == NULL) {
        set_error("out of memory");
        return NULL;
    }
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, text, count, NULL, NULL);
    return text;
}

// A function to give other Windows code a UTF-16 copy of a UTF-8 string, such as a command to launch
wchar_t *fileio_wide(const char *text)
{
    return to_wide(text);
}
#endif

// A function to open a file whose path is UTF-8
FILE *fileio_open(const char *path, const char *mode)
{
#ifdef _WIN32
    wchar_t *wide_path = to_wide(path);
    wchar_t *wide_mode = to_wide(mode);
    FILE *file = NULL;
    if (wide_path != NULL && wide_mode != NULL) {
        // _wfsopen() with _SH_DENYNO shares the file as _wfopen() did; _wfopen_s() would lock
        // other programs out of it while it is open
        file = _wfsopen(wide_path, wide_mode, _SH_DENYNO);
        if (file == NULL)
            set_errno_error(errno);
    }
    alloc_free(wide_path);
    alloc_free(wide_mode);
    return file;
#else
    FILE *file = fopen(path, mode);
    if (file == NULL)
        set_errno_error(errno);
    return file;
#endif
}

// A function to tell whether a file or folder exists and can be read; when not, it says why
bool fileio_exists(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    if (wide == NULL)
        return false;
    bool exists = _waccess(wide, 4) == 0;
    int code = errno;
    alloc_free(wide);
    if (!exists)
        set_errno_error(code);
    return exists;
#else
    if (access(path, R_OK) == 0)
        return true;
    set_errno_error(errno);
    return false;
#endif
}

// A function to tell whether anything is at a path, whether or not it can be read; when not, it
// says why
bool fileio_present(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    if (wide == NULL)
        return false;
    DWORD attributes = GetFileAttributesW(wide);
    DWORD code = GetLastError();
    alloc_free(wide);
    if (attributes != INVALID_FILE_ATTRIBUTES)
        return true;
    set_windows_error(code);
    return false;
#else
    struct stat info;
    if (stat(path, &info) == 0)
        return true;
    set_errno_error(errno);
    return false;
#endif
}

// A function to tell whether a path is a folder; when not, it says why
bool fileio_is_dir(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    if (wide == NULL)
        return false;
    DWORD attributes = GetFileAttributesW(wide);
    DWORD code = GetLastError();
    alloc_free(wide);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        set_windows_error(code);
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        set_error("not a folder");
        return false;
    }
    return true;
#else
    struct stat info;
    if (stat(path, &info) != 0) {
        set_errno_error(errno);
        return false;
    }
    if (!S_ISDIR(info.st_mode)) {
        set_error("not a folder");
        return false;
    }
    return true;
#endif
}

// A function to tell whether an existing file can be opened for writing
bool fileio_is_writable(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    if (wide == NULL)
        return false;
    HANDLE handle = CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD code = GetLastError();
    alloc_free(wide);
    if (handle == INVALID_HANDLE_VALUE) {
        // Another program holding it is a moment's wait, which fileio_replace() handles
        if (code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION)
            return true;
        set_windows_error(code);
        return false;
    }
    CloseHandle(handle);
    return true;
#else
    if (access(path, W_OK) == 0)
        return true;
    set_errno_error(errno);
    return false;
#endif
}

// A function to read a whole file into a new NUL-terminated buffer
char *fileio_read_all(const char *path, size_t *length)
{
    FILE *file = fileio_open(path, "rb");
    if (file == NULL)
        return NULL;
    size_t capacity = 4096;
    size_t used = 0;
    char *buffer = alloc_malloc(capacity + 1);
    while (buffer != NULL) {
        used += fread(buffer + used, 1, capacity - used, file);
        if (used < capacity)
            break;
        capacity *= 2;
        char *bigger = alloc_realloc(buffer, capacity + 1);
        if (bigger == NULL) {
            alloc_free(buffer);
            buffer = NULL;
        }
        else
            buffer = bigger;
    }
    bool failed = ferror(file) != 0;
    fclose(file);
    if (buffer == NULL || failed) {
        alloc_free(buffer);
        set_error(failed ? "the file could not be read" : "out of memory");
        return NULL;
    }
    buffer[used] = '\0';
    if (length != NULL)
        *length = used;
    return buffer;
}

// A function to write a whole file and flush it to the disk before closing it
bool fileio_write_all(const char *path, const char *data, size_t length)
{
    FILE *file = fileio_open(path, "wb");
    if (file == NULL)
        return false;
    bool ok = fwrite(data, 1, length, file) == length && fflush(file) == 0;
#ifdef _WIN32
    ok = ok && _commit(_fileno(file)) == 0;
#else
    ok = ok && fsync(fileno(file)) == 0;
#endif
    if (!ok)
        set_errno_error(errno);
    if (fclose(file) != 0 && ok) {
        set_errno_error(errno);
        ok = false;
    }
    return ok;
}

// A function to copy a file (config files are small, so it goes through memory)
bool fileio_copy(const char *from, const char *to)
{
    size_t length = 0;
    char *data = fileio_read_all(from, &length);
    if (data == NULL)
        return false;
    bool ok = fileio_write_all(to, data, length);
    alloc_free(data);
    return ok;
}

// A function to put one file in place of another in a single step. On Windows, antivirus
// scanners and indexers open a file that has just changed, and the replace is refused while
// they hold it, so it is tried again for about a second. What the new file could not take over
// from the old one does not fail the replace; fileio_last_warning() says what it was.
bool fileio_replace(const char *from, const char *to)
{
    last_warning[0] = '\0';
#ifdef _WIN32
    wchar_t *wide_from = to_wide(from);
    wchar_t *wide_to = to_wide(to);

    // The new file takes the old one's hidden and system attributes, so a file the user hid stays
    // hidden. Read-only is not carried over: a read-only file is refused before it gets here.
    DWORD kept = 0;
    if (wide_from != NULL && wide_to != NULL) {
        DWORD attributes = GetFileAttributesW(wide_to);
        if (attributes != INVALID_FILE_ATTRIBUTES)
            kept = attributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    }
    bool ok = false;
    for (int attempt = 0; wide_from != NULL && wide_to != NULL && attempt < FILEIO_REPLACE_ATTEMPTS; attempt++) {
        if (MoveFileExW(wide_from, wide_to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            ok = true;
            break;
        }
        DWORD code = GetLastError();
        set_windows_error(code);
        if (code != ERROR_SHARING_VIOLATION && code != ERROR_LOCK_VIOLATION && code != ERROR_ACCESS_DENIED)
            break;
        Sleep(FILEIO_REPLACE_WAIT_MS);
    }

    // FILE_ATTRIBUTE_NORMAL is only valid alone, so it is dropped beside the kept attributes
    if (ok && (kept != 0 || fault == FILEIO_FAULT_KEEP)) {
        DWORD attributes = GetFileAttributesW(wide_to);
        if (attributes == INVALID_FILE_ATTRIBUTES || fault == FILEIO_FAULT_KEEP ||
            !SetFileAttributesW(wide_to, (attributes & ~(DWORD) FILE_ATTRIBUTE_NORMAL) | kept))
            snprintf(last_warning, sizeof(last_warning), "the file's hidden or system attribute could not be kept");
    }
    alloc_free(wide_from);
    alloc_free(wide_to);
    return ok;
#else
    // The new file takes the old one's permission bits. Each file is opened once and its bits are
    // read and set through that descriptor, so no other file can be put at either path between a
    // look and a change. O_PATH reads the bits of a file the user cannot read, as stat() did, and
    // O_NONBLOCK keeps a pipe put at a path from blocking the open. No old file keeps nothing.
    // fchmod() refuses an O_PATH descriptor (EBADF), so the new file is opened for writing, or for
    // reading when a umask left its owner no write bit: a umask that takes away either bit still
    // lets the bits be set, as chmod() by path did.
#ifdef O_PATH
    int old_file = open(to, O_PATH | O_CLOEXEC);
#else
    int old_file = open(to, O_RDONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
#endif
    bool mode_kept = old_file < 0 && errno == ENOENT;
    if (old_file >= 0) {
        struct stat info;
        int new_file = -1;
        if (fault != FILEIO_FAULT_KEEP && fstat(old_file, &info) == 0) {
            new_file = open(from, O_WRONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
            if (new_file < 0 && errno == EACCES)
                new_file = open(from, O_RDONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
            mode_kept = new_file >= 0 && fchmod(new_file, info.st_mode & 07777) == 0;
        }
        if (new_file >= 0)
            close(new_file);
        close(old_file);
    }
    if (rename(from, to) != 0) {
        set_errno_error(errno);
        return false;
    }
    if (!mode_kept)
        snprintf(last_warning, sizeof(last_warning), "the file's permissions could not be kept");
    return true;
#endif
}

// A function to delete a file
bool fileio_remove(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    if (wide == NULL)
        return false;
    bool ok = DeleteFileW(wide) != 0;
    DWORD code = GetLastError();
    alloc_free(wide);
    if (!ok)
        set_windows_error(code);
    return ok;
#else
    if (remove(path) == 0)
        return true;
    set_errno_error(errno);
    return false;
#endif
}

// A function to make one folder, which may exist already
static bool make_dir(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    if (wide == NULL)
        return false;
    bool ok = CreateDirectoryW(wide, NULL) != 0;
    DWORD code = GetLastError();
    alloc_free(wide);
    if (ok || code == ERROR_ALREADY_EXISTS)
        return true;
    set_windows_error(code);
    return false;
#else
    if (mkdir(path, 0755) == 0 || errno == EEXIST)
        return true;
    set_errno_error(errno);
    return false;
#endif
}

// A function to make a folder and every folder above it that is missing
bool fileio_make_dirs(const char *path)
{
    size_t length = strlen(path);
    char *buffer = alloc_malloc(length + 1);
    if (buffer == NULL) {
        set_error("out of memory");
        return false;
    }
    memcpy(buffer, path, length + 1);

    // Make each parent in turn: cut the path at each separator, skipping a leading one ("/") and a
    // drive's ("C:\"), and on Windows a share ("\\server\share"), which is there or not but cannot
    // be made
    size_t start = 1;
#ifdef _WIN32
    if (length >= 2 && fileio_is_separator(buffer[0]) && fileio_is_separator(buffer[1])) {
        int parts = 0;
        for (start = 2; start < length && parts < 2; start++) {
            if (fileio_is_separator(buffer[start]))
                parts++;
        }

        // `start` is now past the separator after the share. A path that is only the share has
        // nothing to make, and is not looked at: looking waits on the network when the server is
        // off. Whatever is done there next says so, if it is not there.
        if (parts < 2 || start >= length) {
            alloc_free(buffer);
            return true;
        }
    }
#endif
    for (size_t i = start; i < length; i++) {
        if (fileio_is_separator(buffer[i]) && buffer[i - 1] != ':') {
            char separator = buffer[i];
            buffer[i] = '\0';
            if (!make_dir(buffer)) {
                alloc_free(buffer);
                return false;
            }
            buffer[i] = separator;
        }
    }
    bool ok = make_dir(buffer);
    alloc_free(buffer);
    return ok;
}

// A function to copy a path into the caller's buffer. One that does not fit is refused and left
// empty, so a caller that ignores the result never uses a shorter path naming a different file.
static bool copy_path(char *out, size_t size, const char *path)
{
    int written = snprintf(out, size, "%s", path);
    if (written < 0 || written >= (int) size) {
        if (size > 0)
            out[0] = '\0';
        set_error("the path is too long");
        return false;
    }
    return true;
}

// A function to find the file a path really names: on Linux a symbolic link is followed, so a
// save writes the file it points to and leaves the link in place
bool fileio_real_path(const char *path, char *out, size_t size)
{
#ifdef _WIN32
    return copy_path(out, size, path);
#else
    char resolved[PATH_MAX];
    if (realpath(path, resolved) == NULL) {
        // Say why it could not be resolved, unless the path did not even fit
        int saved = errno;
        if (copy_path(out, size, path))
            set_errno_error(saved);
        return false;
    }
    return copy_path(out, size, resolved);
#endif
}

// A function to find the absolute path a file's name stands for, so a name found from the working
// folder (".\config.ini") still says where the file is: on Linux its real path, as above; on Windows
// the working folder's path joined to it, whether or not the file exists
bool fileio_full_path(const char *path, char *out, size_t size)
{
#ifdef _WIN32
    if (size > 0)
        out[0] = '\0';
    wchar_t *wide = to_wide(path);
    if (wide == NULL)
        return false;
    char *text = NULL;
    DWORD needed = GetFullPathNameW(wide, 0, NULL, NULL);
    if (needed == 0)
        set_windows_error(GetLastError());
    else {
        wchar_t *full = alloc_malloc((size_t) needed * sizeof(wchar_t));
        DWORD written = full != NULL ? GetFullPathNameW(wide, needed, full, NULL) : 0;
        if (full == NULL)
            set_error("out of memory");
        else if (written == 0 || written >= needed)
            set_windows_error(GetLastError());
        else
            text = to_utf8(full);
        alloc_free(full);
    }
    alloc_free(wide);
    bool ok = text != NULL && copy_path(out, size, text);
    alloc_free(text);
    return ok;
#else
    return fileio_real_path(path, out, size);
#endif
}

#ifndef _WIN32
#define MAX_LINKS 8   // How many links deep a link is followed by its text

// File systems whose server can be switched off. Any FUSE one counts too ("fuse.sshfs", or plain
// "fuse" from older sshfs), since its daemon can hang.
static const char *const NETWORK_TYPES[] = {
    "fuse", "nfs", "nfs4", "cifs", "smb3", "smbfs", "ncpfs", "afs", "9p", "ceph", "glusterfs", "davfs", "autofs"
};

// The mount table read, which the unit tests replace with a pretend one
static const char *mount_table = "/proc/self/mounts";

// A function for the unit tests to read the mounts from a file of their own; NULL goes back to
// /proc/self/mounts. The path must stay valid while it is in use.
void fileio_set_mount_table(const char *path)
{
    mount_table = path != NULL ? path : "/proc/self/mounts";
}

// A function to tell whether a path is a folder or inside it: "/mnt" and "/mnt/nas" are in "/mnt",
// "/mntx" is not
static bool is_in(const char *path, const char *folder)
{
    size_t length = strlen(folder);
    if (length == 0 || strncmp(path, folder, length) != 0)
        return false;
    return folder[length - 1] == '/' || path[length] == '\0' || path[length] == '/';
}

// A function to tell a network file system by its type in the mount table
static bool is_network_type(const char *type)
{
    bool network = strncmp(type, "fuse.", 5) == 0;
    for (size_t i = 0; !network && i < sizeof(NETWORK_TYPES) / sizeof(NETWORK_TYPES[0]); i++)
        network = strcmp(type, NETWORK_TYPES[i]) == 0;
    return network;
}

// A function to tell a path on a network file system, from the mount table: the type of the mount
// with the longest path that holds it. Reading /proc/self/mounts touches none of the mounts.
static bool on_network_mount(const char *path)
{
    FILE *table = setmntent(mount_table, "r");
    if (table == NULL)
        return false;
    struct mntent mount;
    char buffer[4096];
    size_t longest = 0;
    bool network = false;
    while (getmntent_r(table, &mount, buffer, (int) sizeof(buffer)) != NULL) {
        size_t length = strlen(mount.mnt_dir);
        if (length < longest || !is_in(path, mount.mnt_dir))
            continue;
        longest = length;   // On a tie the later mount wins: it hides the earlier one
        network = is_network_type(mount.mnt_type);
    }
    endmntent(table);
    return network;
}

// A function to give the name of the entry of `folder` that a mount's path is, or NULL when it is
// not one of the folder's entries. `folder` is a real path, as the table's are.
static const char *entry_mounted_on(const char *path, const char *folder)
{
    if (!is_in(path, folder))
        return NULL;
    const char *entry = path + strlen(folder);
    while (*entry == '/')
        entry++;
    return entry[0] != '\0' && strchr(entry, '/') == NULL ? entry : NULL;
}

// A function to tell, from the mount table alone, whether a network file system is mounted on the
// entry `name` of a folder. Of two mounts on one entry, the later hides the earlier.
static bool network_mount_on(const char *folder, const char *name)
{
    FILE *table = setmntent(mount_table, "r");
    if (table == NULL)
        return false;
    struct mntent mount;
    char buffer[4096];
    bool network = false;
    while (getmntent_r(table, &mount, buffer, (int) sizeof(buffer)) != NULL) {
        const char *entry = entry_mounted_on(mount.mnt_dir, folder);
        if (entry != NULL && strcmp(entry, name) == 0)
            network = is_network_type(mount.mnt_type);
    }
    endmntent(table);
    return network;
}

#define MAX_NESTED_MOUNTS 8   // How many network mounts on a folder's entries a listing keeps by name

// The entries of a folder that a network file system is mounted on, from one read of the mount table
typedef struct {
    int count;   // -1 when there were more than MAX_NESTED_MOUNTS: then each entry is found in the table
    char names[MAX_NESTED_MOUNTS][NAME_MAX + 1];
} NestedMounts;

// A function to read the mount table once for the entries of a folder that a network file system is
// mounted on. Of two mounts on one entry, the later hides the earlier.
static void find_nested_mounts(const char *folder, NestedMounts *nested)
{
    nested->count = 0;
    FILE *table = setmntent(mount_table, "r");
    if (table == NULL)
        return;
    struct mntent mount;
    char buffer[4096];
    while (nested->count >= 0 && getmntent_r(table, &mount, buffer, (int) sizeof(buffer)) != NULL) {
        const char *entry = entry_mounted_on(mount.mnt_dir, folder);
        if (entry == NULL)
            continue;
        int found = -1;
        for (int i = 0; found < 0 && i < nested->count; i++) {
            if (strcmp(nested->names[i], entry) == 0)
                found = i;
        }
        bool network = is_network_type(mount.mnt_type);
        if (network && found < 0) {
            if (nested->count == MAX_NESTED_MOUNTS || strlen(entry) > NAME_MAX)
                nested->count = -1;
            else
                snprintf(nested->names[nested->count++], sizeof(nested->names[0]), "%s", entry);
        }
        else if (!network && found >= 0) {
            nested->count--;
            if (found != nested->count)
                memcpy(nested->names[found], nested->names[nested->count], sizeof(nested->names[0]));
        }
    }
    endmntent(table);
}

// A function to tell whether a network file system is mounted on the entry `name` of a folder, from
// the mounts found there, or from the table when there were too many to keep
static bool is_nested_mount(const NestedMounts *nested, const char *folder, const char *name)
{
    if (nested->count < 0)
        return network_mount_on(folder, name);
    for (int i = 0; i < nested->count; i++) {
        if (strcmp(nested->names[i], name) == 0)
            return true;
    }
    return false;
}

// A function to take "." and ".." out of an absolute path by its text alone, in place, as a link's
// text often has them ("../../mnt/nas")
static void tidy_path(char *path)
{
    size_t out = 0;
    size_t i = 0;
    while (path[i] != '\0') {
        while (path[i] == '/')
            i++;
        size_t start = i;
        while (path[i] != '\0' && path[i] != '/')
            i++;
        size_t length = i - start;
        if (length == 0 || (length == 1 && path[start] == '.'))
            continue;
        if (length == 2 && path[start] == '.' && path[start + 1] == '.') {
            while (out > 0 && path[out - 1] != '/')
                out--;
            if (out > 0)
                out--;
            continue;
        }
        path[out++] = '/';
        memmove(path + out, path + start, length);
        out += length;
    }
    if (out == 0)
        path[out++] = '/';
    path[out] = '\0';
}

// A function to put, in place of a link's path, the absolute path its text names, without looking
// at what is there: readlink() reads only the link. A relative link is read from the link's own
// folder, and a relative folder from the working folder. False when the path is not a link, or the
// result does not fit.
static bool read_link(char *path, size_t size)
{
    char target[PATH_MAX];
    ssize_t length = readlink(path, target, sizeof(target) - 1);
    if (length < 0)
        return false;
    target[length] = '\0';
    char joined[3 * PATH_MAX];
    char working[PATH_MAX] = "";
    if (target[0] == '/')
        snprintf(joined, sizeof(joined), "%s", target);
    else {
        const char *slash = strrchr(path, '/');
        int folder = slash != NULL ? (int) (slash - path) + 1 : 0;
        if (path[0] != '/' && getcwd(working, sizeof(working)) == NULL)
            return false;
        snprintf(joined, sizeof(joined), "%s%s%.*s%s", working, working[0] != '\0' ? "/" : "", folder, path, target);
    }
    tidy_path(joined);
    if (strlen(joined) >= size)
        return false;
    snprintf(path, size, "%s", joined);
    return true;
}

// A function to tell whether a link leads onto a network file system, following it by its text
// alone a few links deep
static bool link_leads_to_network(const char *link)
{
    char current[PATH_MAX];
    if (snprintf(current, sizeof(current), "%s", link) >= (int) sizeof(current))
        return false;
    for (int links = 0; links < MAX_LINKS && read_link(current, sizeof(current)); links++) {
        if (on_network_mount(current))
            return true;
    }
    return false;
}
#endif

// A function to add one entry to a growing list, which takes over `name`; false when out of memory,
// with the reason set and the list as it was
static bool add_entry(FileioEntry **entries, int *count, int *capacity, char *name, bool is_dir, bool hidden)
{
    if (name == NULL)
        return false;
    if (*count == *capacity) {
        int grown = *capacity ? *capacity * 2 : 32;
        FileioEntry *bigger = alloc_realloc(*entries, (size_t) grown * sizeof(FileioEntry));
        if (bigger == NULL) {
            alloc_free(name);
            set_error("out of memory");
            return false;
        }
        *entries = bigger;
        *capacity = grown;
    }
    (*entries)[*count] = (FileioEntry) { .name = name, .is_dir = is_dir, .hidden = hidden };
    (*count)++;
    return true;
}

// A function to list a folder's files and folders, without "." and "..". It is -1, with the
// reason, when the folder cannot be listed whole: never a shorter list.
int fileio_list(const char *folder, FileioEntry **entries)
{
    int count = 0;
    int capacity = 0;
    bool ok = true;
    *entries = NULL;
#ifdef _WIN32
    size_t length = strlen(folder);
    char *pattern = alloc_malloc(length + 3);
    if (pattern == NULL) {
        set_error("out of memory");
        return -1;
    }
    bool separator = length > 0 && fileio_is_separator(folder[length - 1]);
    snprintf(pattern, length + 3, "%s%s*", folder, separator ? "" : "\\");
    wchar_t *wide = to_wide(pattern);
    alloc_free(pattern);
    if (wide == NULL)
        return -1;
    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW(wide, &data);
    DWORD code = GetLastError();
    alloc_free(wide);
    if (handle == INVALID_HANDLE_VALUE) {
        if (code == ERROR_FILE_NOT_FOUND)
            return 0;
        set_windows_error(code);
        return -1;
    }
    while (ok) {
        if (wcscmp(data.cFileName, L".") != 0 && wcscmp(data.cFileName, L"..") != 0) {
            ok = add_entry(entries, &count, &capacity, to_utf8(data.cFileName),
                           (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
                           (data.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0);
            if (!ok)
                break;
        }

        // Only the end of the folder ends the listing: a share that drops partway fails it
        BOOL more = FindNextFileW(handle, &data);
        DWORD read_code = more ? ERROR_SUCCESS : GetLastError();
        if (more && fault == FILEIO_FAULT_LIST_READ && count >= fault_after) {
            more = FALSE;
            read_code = (DWORD) fault_code;
        }
        if (!more) {
            if (read_code != ERROR_NO_MORE_FILES) {
                set_windows_error(read_code);
                ok = false;
            }
            break;
        }
    }
    FindClose(handle);
#else
    DIR *dir = opendir(folder);
    if (dir == NULL) {
        if (errno == ENOTDIR)
            set_error("not a folder");
        else
            set_errno_error(errno);
        return -1;
    }
    size_t folder_length = strlen(folder);
    char real[PATH_MAX] = "";               // The folder's real path, for the mount table...
    NestedMounts nested = { .count = 0 };   // ...and the network mounts on its entries...
    bool nested_read = false;               // ...read at the first entry of unknown kind
    while (ok) {
        // readdir() gives NULL at the end and on an error, which only errno tells apart; only the
        // end ends the listing
        errno = 0;
        struct dirent *entry = readdir(dir);
        int read_code = errno;
        if (entry != NULL && fault == FILEIO_FAULT_LIST_READ && count >= fault_after) {
            entry = NULL;
            read_code = fault_code;
        }
        if (entry == NULL) {
            if (read_code != 0) {
                set_errno_error(read_code);
                ok = false;
            }
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // The listing says what each entry is, so a folder holding a network mount whose server is
        // off is listed without touching the mount: stat() on it would block, on a hard NFS mount
        // for good. A link is followed, since a link to a folder should open like one; but one whose
        // text leads onto a network mount is taken for a folder unlooked. An entry of unknown kind
        // (some CIFS, NFS and older XFS mounts give none) costs one lstat(), which looks at the entry
        // itself and never follows it; one that turns out to be a link is then treated as a link.
        // But an lstat() of an entry that a file system is mounted on looks at that file system's
        // root, so the mount table is read first, once, by the folder's real path (again for each
        // entry only in a folder with more than MAX_NESTED_MOUNTS network mounts on its entries),
        // and an entry that a network file system is mounted on is taken for a folder unlooked. So
        // a folder of 300 such entries is 300 lookups, on the file system being listed, and none
        // beyond it.
        unsigned char kind = fault == FILEIO_FAULT_NO_KIND ? (unsigned char) DT_UNKNOWN : entry->d_type;
        bool is_dir = kind == DT_DIR;
        if (kind == DT_LNK || kind == DT_UNKNOWN) {
            size_t size = folder_length + strlen(entry->d_name) + 2;
            char *full = alloc_malloc(size);
            if (full == NULL) {
                set_error("out of memory");
                ok = false;
                break;
            }
            snprintf(full, size, "%s/%s", folder, entry->d_name);
            struct stat info;
            bool link = kind == DT_LNK;
            if (kind == DT_UNKNOWN && !nested_read) {
                // The table names real paths only, so without the folder's (realpath() fails only
                // when it is too long or memory runs out) nothing in it can be matched: the guard is
                // then off, and each entry is looked at as if no mount were on it
                if (realpath(folder, real) != NULL)
                    find_nested_mounts(real, &nested);
                else
                    nested.count = 0;
                nested_read = true;
            }
            if (kind == DT_UNKNOWN && is_nested_mount(&nested, real, entry->d_name))
                is_dir = true;
            else if (kind == DT_UNKNOWN && lstat(full, &info) == 0) {
                link = S_ISLNK(info.st_mode);
                is_dir = S_ISDIR(info.st_mode);
            }
            if (link)
                is_dir = link_leads_to_network(full) || (stat(full, &info) == 0 && S_ISDIR(info.st_mode));
            alloc_free(full);
        }
        char *name = alloc_strdup(entry->d_name);
        if (name == NULL)
            set_error("out of memory");
        ok = add_entry(entries, &count, &capacity, name, is_dir, entry->d_name[0] == '.');
    }
    closedir(dir);
#endif
    if (!ok) {
        fileio_free_list(*entries, count);
        *entries = NULL;
        return -1;
    }
    return count;
}

// A function to free a list from fileio_list
void fileio_free_list(FileioEntry *entries, int count)
{
    for (int i = 0; i < count; i++)
        alloc_free(entries[i].name);
    alloc_free(entries);
}

// The places found so far, and whether finding them failed on the way, and why
typedef struct {
    FileioPlace *items;
    int count;
    bool failed;
    char why[64];
} PlaceList;

// A function to fail the list with a reason, so the caller gets no list rather than one with a
// place quietly missing
static void fail_places(PlaceList *list, const char *why)
{
    list->failed = true;
    snprintf(list->why, sizeof(list->why), "%s", why);
}

// A function to add a starting place without looking at its folder. Out of memory, the list fails.
static void append_place(PlaceList *list, const char *label, const char *path, bool network)
{
    if (path == NULL)
        return;
    char *label_copy = alloc_strdup(label);
    char *path_copy = alloc_strdup(path);
    FileioPlace *grown = NULL;
    if (label_copy != NULL && path_copy != NULL)
        grown = alloc_realloc(list->items, (size_t) (list->count + 1) * sizeof(FileioPlace));
    if (grown == NULL) {
        alloc_free(label_copy);
        alloc_free(path_copy);
        fail_places(list, "out of memory");
        return;
    }
    list->items = grown;
    list->items[list->count] = (FileioPlace) { .label = label_copy, .path = path_copy, .network = network };
    list->count++;
}

// A function to hand the caller the places, or, when finding them failed, none: -1 with the reason
static int finish_places(PlaceList *list, FileioPlace **places)
{
    if (list->failed) {
        fileio_free_places(list->items, list->count);
        *places = NULL;
        set_error(list->why);
        return -1;
    }
    *places = list->items;
    return list->count;
}

#ifdef _WIN32
// A function to tell a path on a network share: a UNC path ("\\server\share") or a drive letter
// mapped to one. GetDriveTypeW answers for a drive's root from the drive map, without touching the
// drive, so it cannot wait on the network.
static bool is_network_path(const char *path)
{
    if (fileio_is_separator(path[0]) && fileio_is_separator(path[1]))
        return true;
    if (path[0] == '\0' || path[1] != ':')
        return false;
    wchar_t root[4] = { (wchar_t) (unsigned char) path[0], L':', L'\\', L'\0' };
    return GetDriveTypeW(root) == DRIVE_REMOTE;
}
#else
// A function to tell a path that may be on a network share: in /media or /mnt, where drives and
// shares are mounted, or on a network file system. A link counts by where its text leads, a few
// links deep (a Pictures folder linked onto a NAS), since looking where it leads is what blocks.
static bool is_network_path(const char *path)
{
    char current[PATH_MAX];
    if (snprintf(current, sizeof(current), "%s", path) >= (int) sizeof(current))
        return false;
    for (int links = 0; links <= MAX_LINKS; links++) {
        if (is_in(current, "/media") || is_in(current, "/mnt") || on_network_mount(current))
            return true;
        if (!read_link(current, sizeof(current)))
            return false;
    }
    return false;
}
#endif

// A function to add a starting place when its folder exists. A folder that may be on a network
// share is added without looking, because looking can wait for the network (see fileio_places).
static void add_place(PlaceList *list, const char *label, const char *path)
{
    if (path == NULL)
        return;
    bool network = is_network_path(path);
    if (network || fileio_is_dir(path))
        append_place(list, label, path, network);
    else if (strcmp(last_error, "out of memory") == 0)
        fail_places(list, "out of memory");   // Not "not there": the look itself ran out of memory
}

#ifndef _WIN32
// A function to add each folder in a folder as a place: the drives and shares mounted in /media and
// /mnt. Only the folder itself is read, never anything in it, not even with stat(): on a network
// mount whose server is off that blocks, and on a hard NFS mount it never returns. So a name the
// folder lists as a folder, a link or of unknown kind is taken on trust. A folder that is itself a
// network mount is listed whole, unread. The user's own folder in it (/media/<user>, where desktops
// mount drives) is read the same way, one level, for the drives inside it; a user is found only from
// USER or LOGNAME, since asking the user database can itself go over the network. Every place found
// here may be on a network share, so each is marked so. False when the folder cannot be read.
static bool add_places_under(PlaceList *list, const char *folder, bool user_folder)
{
    if (on_network_mount(folder)) {
        append_place(list, folder, folder, true);
        return true;
    }
    DIR *dir = opendir(folder);
    if (dir == NULL)
        return false;
    const char *user = getenv("USER");
    if (user == NULL || user[0] == '\0')
        user = getenv("LOGNAME");
    size_t folder_length = strlen(folder);
    struct dirent *entry;
    while (!list->failed && (entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')   // ".", ".." and hidden folders
            continue;
        if (entry->d_type != DT_DIR && entry->d_type != DT_LNK && entry->d_type != DT_UNKNOWN)
            continue;
        size_t size = folder_length + strlen(entry->d_name) + 2;
        char *path = alloc_malloc(size);
        if (path == NULL) {
            fail_places(list, "out of memory");
            break;
        }
        snprintf(path, size, "%s/%s", folder, entry->d_name);

        // Only a real folder is gone into: a link could lead anywhere, a mount included
        bool mine = user_folder && user != NULL && user[0] != '\0' && entry->d_type == DT_DIR &&
                    strcmp(entry->d_name, user) == 0;
        if (!mine || !add_places_under(list, path, false))
            append_place(list, entry->d_name, path, is_network_path(path));
        alloc_free(path);
    }
    closedir(dir);
    return true;
}

// A function to list each folder in a folder as a place, the way /media and /mnt are listed; -1
// with the reason when memory runs out
int fileio_places_under(const char *folder, FileioPlace **places)
{
    PlaceList list = { NULL, 0, false, "" };
    add_places_under(&list, folder, true);
    return finish_places(&list, places);
}

// A function to read a user-dirs.dirs value: "$HOME/<path>" or "/<path>" in double quotes, with a
// backslash before any character that stands for itself
static bool read_user_dir(const char *value, const char *home, char *out, size_t size)
{
    char path[PATH_MAX];
    size_t used = 0;
    if (*value++ != '"')
        return false;
    if (strncmp(value, "$HOME", 5) == 0 && (value[5] == '/' || value[5] == '"')) {
        int written = snprintf(path, sizeof(path), "%s", home);
        if (written < 0 || written >= (int) sizeof(path))
            return false;
        used = (size_t) written;
        value += 5;
    }
    else if (*value != '/')
        return false;
    for (; *value != '\0' && *value != '"'; value++) {
        if (*value == '\\' && value[1] != '\0')
            value++;
        if (used + 1 >= sizeof(path))
            return false;
        path[used++] = *value;
    }
    if (*value != '"')
        return false;
    path[used] = '\0';

    // Set to Home itself, the folder is turned off
    size_t home_length = strlen(home);
    if (strncmp(path, home, home_length) == 0 && (path[home_length] == '\0' || strcmp(path + home_length, "/") == 0))
        return false;
    return snprintf(out, size, "%s", path) < (int) size;
}

// A function to find the Pictures folder: XDG_PICTURES_DIR, else its line in the desktop's
// user-dirs.dirs, which names it in the desktop's language ("$HOME/Bilder"), else ~/Pictures. A
// user-dirs.dirs that cannot be read for want of memory fails the list, rather than falling back
// to a Pictures folder the desktop does not use.
static bool find_pictures(PlaceList *list, const char *home, char *out, size_t size)
{
    const char *variable = getenv("XDG_PICTURES_DIR");
    if (variable != NULL && variable[0] != '\0')
        return snprintf(out, size, "%s", variable) < (int) size;
    if (home == NULL)
        return false;
    char file[PATH_MAX];
    const char *config = getenv("XDG_CONFIG_HOME");
    if (config != NULL && config[0] == '/')
        snprintf(file, sizeof(file), "%s/user-dirs.dirs", config);
    else
        snprintf(file, sizeof(file), "%s/.config/user-dirs.dirs", home);

    // The file is shell: the last line setting the folder wins
    bool found = false;
    char *text = fileio_read_all(file, NULL);
    if (text == NULL && strcmp(last_error, "out of memory") == 0) {
        fail_places(list, "out of memory");
        return false;
    }
    for (char *line = text; line != NULL && *line != '\0';) {
        char *next = strchr(line, '\n');
        if (next != NULL)
            *next++ = '\0';
        while (*line == ' ' || *line == '\t')
            line++;
        if (strncmp(line, "XDG_PICTURES_DIR=", 17) == 0)
            found = read_user_dir(line + 17, home, out, size);
        line = next;
    }
    alloc_free(text);
    return found || snprintf(out, size, "%s/Pictures", home) < (int) size;
}
#endif

// A function to list where the folder browser can start: Pictures first, then Home, then the
// drives (Windows) or the file system's root and what is mounted in /media and /mnt (elsewhere).
// A place that may be on a network share (a network drive or share, a known folder on one, or
// anything in /media or /mnt) is listed without being looked at, and marked: when its server is
// off, looking blocks until the network times out (Windows tries to reconnect), and on a hard NFS
// mount it never returns, while the browser asks for its places each time it opens. This is
// deliberate: one that cannot be listed is refused only when chosen, like any other folder the
// browser cannot list. -1, with the reason, when memory runs out (or a known folder's path cannot be
// converted to UTF-8): never a list with a place missing.
int fileio_places(FileioPlace **places)
{
    PlaceList list = { NULL, 0, false, "" };
#ifdef _WIN32
    // An empty card reader or DVD drive must not pop up "There is no disk in the drive". Only this
    // thread's mode changes: the process's is shared with the launcher's other threads.
    DWORD old_mode = GetThreadErrorMode();
    SetThreadErrorMode(old_mode | SEM_FAILCRITICALERRORS, NULL);

    // Without KF_FLAG_DONT_VERIFY the shell looks for the folder itself, network or not;
    // add_place() looks only when the folder is local
    static const KNOWNFOLDERID *const FOLDERS[] = { &FOLDERID_Pictures, &FOLDERID_Profile };
    static const char *const LABELS[] = { "Pictures", "Home" };
    for (int i = 0; i < 2 && !list.failed; i++) {
        PWSTR wide = NULL;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERS[i], (DWORD) KF_FLAG_DONT_VERIFY, NULL, &wide))) {
            char *path = to_utf8(wide);
            if (path == NULL)
                fail_places(&list, last_error);   // to_utf8's reason: out of memory, or a path it cannot convert
            add_place(&list, LABELS[i], path);
            alloc_free(path);
        }
        CoTaskMemFree(wide);
    }

    // The drive map tells each drive's kind without touching it. Only a card reader or DVD drive is
    // looked at, to leave out an empty one.
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26 && !list.failed; i++) {
        if ((drives & (1u << i)) == 0)
            continue;
        char path[4] = { (char) ('A' + i), ':', '\\', '\0' };
        char label[3] = { (char) ('A' + i), ':', '\0' };
        wchar_t root[4] = { (wchar_t) (L'A' + i), L':', L'\\', L'\0' };
        switch (GetDriveTypeW(root)) {
            case DRIVE_FIXED:
            case DRIVE_RAMDISK:
                append_place(&list, label, path, false);
                break;
            case DRIVE_REMOTE:
                append_place(&list, label, path, true);
                break;
            case DRIVE_REMOVABLE:
            case DRIVE_CDROM:
                add_place(&list, label, path);
                break;
            default:   // DRIVE_NO_ROOT_DIR or DRIVE_UNKNOWN
                break;
        }
    }
    SetThreadErrorMode(old_mode, NULL);
#else
    // HOME only, never the user database, which can go over the network (see add_places_under):
    // with HOME unset the list has no Pictures or Home, and the browser still opens
    const char *home = getenv("HOME");
    char pictures[PATH_MAX];
    if (find_pictures(&list, home, pictures, sizeof(pictures)))
        add_place(&list, "Pictures", pictures);
    add_place(&list, "Home", home);
    add_place(&list, "/", "/");
    add_places_under(&list, "/media", true);
    add_places_under(&list, "/mnt", true);
#endif
    return finish_places(&list, places);
}

// A function to free a list from fileio_places
void fileio_free_places(FileioPlace *places, int count)
{
    for (int i = 0; i < count; i++) {
        alloc_free(places[i].label);
        alloc_free(places[i].path);
    }
    alloc_free(places);
}
