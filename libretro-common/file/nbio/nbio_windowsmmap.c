/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (nbio_windowsmmap.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(_WIN32) && !defined(_XBOX)

#include <stdio.h>
#include <stdlib.h>

#include <file/nbio.h>
#include <encodings/utf.h> 

#include <windows.h>

/* Assume W-functions do not work below VC2005 and Xbox platforms */
#if defined(_MSC_VER) && _MSC_VER < 1400 || defined(_XBOX)

#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif

#endif

#ifndef FILE_SHARE_ALL
#define FILE_SHARE_ALL (FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE)
#endif

struct nbio_mmap_win32_t
{
   HANDLE file;
   bool is_write;
   size_t len;
   void* ptr;
};

static void *nbio_mmap_win32_open(const char * filename, unsigned mode)
{
   static const DWORD dispositions[] = { OPEN_EXISTING, CREATE_ALWAYS, OPEN_ALWAYS, OPEN_EXISTING, CREATE_ALWAYS };
   HANDLE mem;
   LARGE_INTEGER len;
   struct nbio_mmap_win32_t* handle  = NULL;
   void* ptr                         = NULL;
   bool is_write                     = (mode == NBIO_WRITE || mode == NBIO_UPDATE || mode == BIO_WRITE);
   DWORD access                      = (is_write ? GENERIC_READ|GENERIC_WRITE : GENERIC_READ);
#if !defined(_WIN32) || defined(LEGACY_WIN32)
   HANDLE file                       = CreateFile(filename, access, FILE_SHARE_ALL, NULL, dispositions[mode], FILE_ATTRIBUTE_NORMAL, NULL);
#else
   wchar_t *filename_wide            = utf8_to_utf16_string_alloc(filename);
   HANDLE file                       = CreateFileW(filename_wide, access, FILE_SHARE_ALL, NULL, dispositions[mode], FILE_ATTRIBUTE_NORMAL, NULL);

   if (filename_wide)
      free(filename_wide);
#endif

   if (file == INVALID_HANDLE_VALUE)
      return NULL;

   GetFileSizeEx(file, &len);

   mem = CreateFileMapping(file, NULL, is_write ? PAGE_READWRITE : PAGE_READONLY, 0, 0, NULL);
   ptr = MapViewOfFile(mem, is_write ? (FILE_MAP_READ|FILE_MAP_WRITE) : FILE_MAP_READ, 0, 0, len.QuadPart);
   CloseHandle(mem);

   handle           = malloc(sizeof(struct nbio_mmap_win32_t));

   handle->file     = file;
   handle->is_write = is_write;
   handle->len      = len.QuadPart;
   handle->ptr      = ptr;

   return handle;
}

static void nbio_mmap_win32_begin_read(void *data)
{
   /* not needed */
}

static void nbio_mmap_win32_begin_write(void *data)
{
   /* not needed */
}

static bool nbio_mmap_win32_iterate(void *data)
{
   /* not needed */
   return true;
}

static void nbio_mmap_win32_resize(void *data, size_t len)
{
   LARGE_INTEGER len_li;
   HANDLE mem;
   struct nbio_mmap_win32_t* handle  = (struct nbio_mmap_win32_t*)data;

   if (!handle)
      return;

   if (len < handle->len)
   {
      /* this works perfectly fine if this check is removed, 
       * but it won't work on other nbio implementations */
      /* therefore, it's blocked so nobody accidentally 
       * relies on it. */
      puts("ERROR - attempted file shrink operation, not implemented");
      abort();
   }

   len_li.QuadPart = len;
   SetFilePointerEx(handle->file, len_li, NULL, FILE_BEGIN);

   if (!SetEndOfFile(handle->file))
   {
      puts("ERROR - couldn't resize file (SetEndOfFile)");
      abort(); /* this one returns void and I can't find any other way for it to report failure */
   }
   handle->len = len;

   UnmapViewOfFile(handle->ptr);
   mem = CreateFileMapping(handle->file, NULL, handle->is_write ? PAGE_READWRITE : PAGE_READONLY, 0, 0, NULL);
   handle->ptr = MapViewOfFile(mem, handle->is_write ? (FILE_MAP_READ|FILE_MAP_WRITE) : FILE_MAP_READ, 0, 0, len);
   CloseHandle(mem);

   if (!handle->ptr)
   {
      puts("ERROR - couldn't resize file (MapViewOfFile)");
      abort();
   }
}

static void *nbio_mmap_win32_get_ptr(void *data, size_t* len)
{
   struct nbio_mmap_win32_t* handle  = (struct nbio_mmap_win32_t*)data;
   if (!handle)
      return NULL;
   if (len)
      *len = handle->len;
   return handle->ptr;
}

static void nbio_mmap_win32_cancel(void *data)
{
   /* not needed */
}

static void nbio_mmap_win32_free(void *data)
{
   struct nbio_mmap_win32_t* handle  = (struct nbio_mmap_win32_t*)data;
   if (!handle)
      return;
   CloseHandle(handle->file);
   UnmapViewOfFile(handle->ptr);
   free(handle);
}

nbio_intf_t nbio_mmap_win32 = {
   nbio_mmap_win32_open,
   nbio_mmap_win32_begin_read,
   nbio_mmap_win32_begin_write,
   nbio_mmap_win32_iterate,
   nbio_mmap_win32_resize,
   nbio_mmap_win32_get_ptr,
   nbio_mmap_win32_cancel,
   nbio_mmap_win32_free,
   "nbio_mmap_win32",
};

#endif
