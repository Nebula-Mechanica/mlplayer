/*
 *  vfs.c
 *  Copyright 2006-2011 William Pitcock, Daniel Barkalow, Ralf Ertzinger,
 *                      Yoshiki Yazawa, Matti Hämäläinen, and John Lindgren
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The MLPlayer team does not consider modular code linking to
 *  MLPlayer or using our public API to be a derived work.
 */

#include <glib.h>
#include <inttypes.h>

#include "vfs.h"
#include "mlpstrings.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#include "config.h"

#define VFS_SIG ('V' | ('F' << 8) | ('S' << 16))

/**
 * @struct _VFSFile
 * #VFSFile objects describe an opened VFS stream, basically being
 * similar in purpose as stdio FILE
 */
struct _VFSFile {
    char * uri;               /**< The URI of the stream */
    VFSConstructor * base;    /**< The base vtable used for VFS functions */
    void * handle;            /**< Opaque data used by the transport plugins */
    int sig;                  /**< Used to detect invalid or twice-closed objects */
};

/* MLPlayer core provides us with a function that looks up a VFS transport for
 * a given URI scheme.  Since this function will load plugins as needed, it can
 * only be called from the main thread.  When VFS is used from parallel threads,
 * vfs_prepare must be called from the main thread to look up any needed
 * transports beforehand. */

static VFSConstructor * (* lookup_func) (const char * scheme) = NULL;

EXPORT void vfs_set_lookup_func (VFSConstructor * (* func) (const char * scheme))
{
    lookup_func = func;
}

static bool_t verbose = FALSE;

EXPORT void vfs_set_verbose (bool_t set)
{
    verbose = set;
}

static void logger (const char * format, ...)
{
    static char last[256] = "";
    static int repeated = 0;

    char buf[256];

    va_list args;
    va_start (args, format);
    vsnprintf (buf, sizeof buf, format, args);
    va_end (args);

    if (! strcmp (buf, last))
        repeated ++;
    else
    {
        if (repeated)
        {
            printf ("VFS: (last message repeated %d times)\n", repeated);
            repeated = 0;
        }

        fputs (buf, stdout);
        strcpy (last, buf);
    }
}

EXPORT VFSFile * vfs_new (const char * path, VFSConstructor * vtable, void * handle)
{
    VFSFile * file = g_slice_new (VFSFile);
    file->uri = str_get (path);
    file->base = vtable;
    file->handle = handle;
    file->sig = VFS_SIG;
    return file;
}

EXPORT const char * vfs_get_filename (VFSFile * file)
{
    return file->uri;
}

EXPORT void * vfs_get_handle (VFSFile * file)
{
    return file->handle;
}

/**
 * Opens a stream from a VFS transport using one of the registered
 * #VFSConstructor handlers.
 *
 * @param path The path or URI to open.
 * @param mode The preferred access privileges (not guaranteed).
 * @return On success, a #VFSFile object representing the stream.
 */
EXPORT VFSFile *
vfs_fopen(const char * path,
          const char * mode)
{
    g_return_val_if_fail (path && mode, NULL);
    g_return_val_if_fail (lookup_func, NULL);

    const char * s = strstr (path, "://");
    g_return_val_if_fail (s, NULL);
    char scheme[s - path + 1];
    strncpy (scheme, path, s - path);
    scheme[s - path] = 0;

    VFSConstructor * vtable = lookup_func (scheme);
    if (! vtable)
        return NULL;

    const gchar * sub;
    uri_parse (path, NULL, NULL, & sub, NULL);

    gchar buf[sub - path + 1];
    memcpy (buf, path, sub - path);
    buf[sub - path] = 0;

    void * handle = vtable->vfs_fopen_impl (buf, mode);
    if (! handle)
        return NULL;

    VFSFile * file = vfs_new (path, vtable, handle);

    if (verbose)
        logger ("VFS: <%p> open (mode %s) %s\n", file, mode, path);

    return file;
}

/**
 * Closes a VFS stream and destroys a #VFSFile object.
 *
 * @param file A #VFSFile object to destroy.
 * @return -1 on failure, 0 on success.
 */
EXPORT int
vfs_fclose(VFSFile * file)
{
    g_return_val_if_fail (file && file->sig == VFS_SIG, -1);

    if (verbose)
        logger ("VFS: <%p> close\n", file);

    int ret = 0;

    if (file->base->vfs_fclose_impl(file) != 0)
        ret = -1;

    str_unref (file->uri);

    memset (file, 0, sizeof (VFSFile));
    g_slice_free (VFSFile, file);

    return ret;
}

/**
 * Reads from a VFS stream.
 *
 * @param ptr A pointer to the destination buffer.
 * @param size The size of each element to read.
 * @param nmemb The number of elements to read.
 * @param file #VFSFile object that represents the VFS stream.
 * @return The number of elements succesfully read.
 */
EXPORT int64_t vfs_fread (void * ptr, int64_t size, int64_t nmemb, VFSFile * file)
{
    g_return_val_if_fail (file && file->sig == VFS_SIG, 0);

    int64_t readed = file->base->vfs_fread_impl (ptr, size, nmemb, file);

/*    if (verbose)
        logger ("VFS: <%p> read %"PRId64" elements of size %"PRId64" = "
         "%"PRId64"\n", file, nmemb, size, readed); */

    return readed;
}

/**
 * Writes to a VFS stream.
 *
 * @param ptr A const pointer to the source buffer.
 * @param size The size of each element to write.
 * @param nmemb The number of elements to write.
 * @param file #VFSFile object that represents the VFS stream.
 * @return The number of elements succesfully written.
 */
EXPORT int64_t vfs_fwrite (const void * ptr, int64_t size, int64_t nmemb, VFSFile * file)
{
    g_return_val_if_fail (file && file->sig == VFS_SIG, 0);

    int64_t written = file->base->vfs_fwrite_impl (ptr, size, nmemb, file);

    if (verbose)
        logger ("VFS: <%p> write %"PRId64" elements of size %"PRId64" = "
         "%"PRId64"\n", file, nmemb, size, written);

    return written;
}

/**
 * Reads a character from a VFS stream.
 *
 * @param file #VFSFile object that represents the VFS stream.
 * @return On success, a character. Otherwise, EOF.
 */
EXPORT int
vfs_getc(VFSFile *file)
{
    g_return_val_if_fail (file && file->sig == VFS_SIG, EOF);

    if (verbose)
        logger ("VFS: <%p> getc\n", file);

    return file->base->vfs_getc_impl(file);
}

/**
 * Pushes a character back to the VFS stream.
 *
 * @param c The character to push back.
 * @param file #VFSFile object that represents the VFS stream.
 * @return On success, 0. Otherwise, EOF.
 */
EXPORT int
vfs_ungetc(int c, VFSFile *file)
{
    g_return_val_if_fail (file && file->sig == VFS_SIG, EOF);

    if (verbose)
        logger ("VFS: <%p> ungetc\n", file);

    return file->base->vfs_ungetc_impl(c, file);
}

/**
 * Performs a seek in given VFS stream. Standard C-style values
 * of whence can be used to indicate desired action.
 *
 * - SEEK_CUR seeks relative to current stream position.
 * - SEEK_SET seeks to given absolute position (relative to stream beginning).
 * - SEEK_END sets stream position to current file end.
 *
 * @param file #VFSFile object that represents the VFS stream.
 * @param offset The offset to seek to.
 * @param whence Type of the seek: SEEK_CUR, SEEK_SET or SEEK_END.
 * @return On success, 0. Otherwise, -1.
 */
EXPORT int
vfs_fseek(VFSFile * file,
          int64_t offset,
          int whence)
{
    g_return_val_if_fail (file && file->sig == VFS_SIG, -1);

    if (verbose)
        logger ("VFS: <%p> seek to %"PRId64" from %s\n", file, offset, whence ==
         SEEK_CUR ? "current" : whence == SEEK_SET ? "beginning" : whence ==
         SEEK_END ? "end" : "invalid");

    return file->base->vfs_fseek_impl(file, offset, whence);
}

/**
 * Rewinds a VFS stream.
 *
 * @param file #VFSFile object that represents the VFS stream.
 */
EXPORT void
vfs_rewind(VFSFile * file)
{
    g_return_if_fail (file && file->sig == VFS_SIG);

    if (verbose)
        logger ("VFS: <%p> rewind\n", file);

    file->base->vfs_rewind_impl(file);
}

/**
 * Returns the current position in the VFS stream's buffer.
 *
 * @param file #VFSFile object that represents the VFS stream.
 * @return On success, the current position. Otherwise, -1.
 */
EXPORT int64_t
vfs_ftell(VFSFile * file)
{
    g_return_val_if_fail (file && file->sig == VFS_SIG, -1);

    int64_t told = file->base->vfs_ftell_impl (file);

    if (verbose)
        logger ("VFS: <%p> tell = %"PRId64"\n", file, told);

    return told;
}

/**
 * Returns whether or not the VFS stream has reached EOF.
 *
 * @param file #VFSFile object that represents the VFS stream.
 * @return On success, whether or not the VFS stream is at EOF. Otherwise, FALSE.
 */
EXPORT bool_t
vfs_feof(VFSFile * file)
{
    g_return_val_if_fail (file && file->sig == VFS_SIG, TRUE);

    bool_t eof = file->base->vfs_feof_impl (file);

    if (verbose)
        logger ("VFS: <%p> eof = %s\n", file, eof ? "yes" : "no");

    return eof;
}

/**
 * Truncates a VFS stream to a certain size.
 *
 * @param file #VFSFile object that represents the VFS stream.
 * @param length The length to truncate at.
 * @return On success, 0. Otherwise, -1.
 */
EXPORT int vfs_ftruncate (VFSFile * file, int64_t length)
{
    g_return_val_if_fail (file && file->sig == VFS_SIG, -1);

    if (verbose)
        logger ("VFS: <%p> truncate to %"PRId64"\n", file, length);

    return file->base->vfs_ftruncate_impl(file, length);
}

/**
 * Returns size of the file.
 *
 * @param file #VFSFile object that represents the VFS stream.
 * @return On success, the size of the file in bytes. Otherwise, -1.
 */
EXPORT int64_t vfs_fsize (VFSFile * file)
{
    g_return_val_if_fail (file && file->sig == VFS_SIG, -1);

    int64_t size = file->base->vfs_fsize_impl (file);

    if (verbose)
        logger ("VFS: <%p> size = %"PRId64"\n", file, size);

    return size;
}

/**
 * Returns metadata about the stream.
 *
 * @param file #VFSFile object that represents the VFS stream.
 * @param field The string constant field name to get.
 * @return On success, a copy of the value of the field. Otherwise, NULL.
 */
EXPORT char *
vfs_get_metadata(VFSFile * file, const char * field)
{
    if (file == NULL)
        return NULL;

    if (file->base->vfs_get_metadata_impl)
        return file->base->vfs_get_metadata_impl(file, field);
    return NULL;
}

/**
 * Wrapper for g_file_test().
 *
 * @param path A path to test.
 * @param test A GFileTest to run.
 * @return The result of g_file_test().
 */
EXPORT bool_t
vfs_file_test(const char * path, int test)
{
    if (strncmp (path, "file://", 7))
        return FALSE; /* only local files are handled */

    char * path2 = uri_to_filename (path);

    if (path2 == NULL)
        path2 = g_strdup(path);

    bool_t ret = g_file_test (path2, test);

    g_free(path2);

    return ret;
}

/**
 * Tests if a file is writeable.
 *
 * @param path A path to test.
 * @return TRUE if the file is writeable, otherwise FALSE.
 */
EXPORT bool_t
vfs_is_writeable(const char * path)
{
    struct stat info;
    char * realfn = uri_to_filename (path);

    if (stat(realfn, &info) == -1)
        return FALSE;

    g_free(realfn);

    return (info.st_mode & S_IWUSR);
}

/**
 * Tests if a path is remote uri.
 *
 * @param path A path to test.
 * @return TRUE if the file is remote, otherwise FALSE.
 */
EXPORT bool_t vfs_is_remote (const char * path)
{
    return strncmp (path, "file://", 7) ? TRUE : FALSE;
}

/**
 * Tests if a file is associated to streaming.
 *
 * @param file A #VFSFile object to test.
 * @return TRUE if the file is streaming, otherwise FALSE.
 */
EXPORT bool_t vfs_is_streaming (VFSFile * file)
{
    return (vfs_fsize (file) < 0);
}
