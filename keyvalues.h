/* This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License

Copyright (c) 2025 Dreamy Cecil

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (https://unlicense.org)

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute
this software, either in source code form or as a compiled binary, for any
purpose, commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef VDF_KEYVALUES_INCL_H
#define VDF_KEYVALUES_INCL_H
#ifdef _WIN32
  #pragma once
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#ifdef __STDC_VERSION__
  #define KV_INLINE static inline
#else
  #define KV_INLINE static
#endif


/* You can provide your own means of memory management by setting all of these functions */
#ifdef VDF_MANAGE_MEMORY
  extern void *(*KV_malloc)(size_t bytes);
  extern void *(*KV_calloc)(size_t ct, size_t elemSize);
  extern void *(*KV_realloc)(void *memory, size_t bytes);
  extern void  (*KV_free)(void *memory);
  extern char *(*KV_strdup)(const char *str);

#else
  /* Use default memory management functions */
  #define KV_malloc  malloc
  #define KV_calloc  calloc
  #define KV_realloc realloc
  #define KV_free    free
  #define KV_strdup  strdup
#endif


/*********************************************************************************************************************************
 * Key-value types
 *********************************************************************************************************************************/


/* Boolean type */
typedef enum _KV_bool {
  KV_false = 0,
  KV_true  = 1,
} KV_bool;


/* Supported data types, synced with KeyValues::types_t from Source SDK 2013 */
typedef enum _KV_DataType {
  KV_TYPE_NONE = 0, /* Acts as a list of subpairs; empty by default */
  KV_TYPE_STRING,
  /* TODO: Support the rest of the types */
  KV_TYPE_INT,
  KV_TYPE_FLOAT,
  KV_TYPE_PTR,
  KV_TYPE_WSTRING,
  KV_TYPE_COLOR,
  KV_TYPE_UINT64,

  KV_TYPE_NUMTYPES,
} KV_DataType;


typedef struct _KV_Printer KV_Printer; /* Context for printing strings in infinite character buffers */
typedef struct _KV_Context KV_Context; /* Parser context for reading VDF contents */
typedef struct _KV_Pair KV_Pair; /* Value of a specific type under a key */


/*********************************************************************************************************************************
 * Error handling
 *********************************************************************************************************************************/


/* Clears any previous error message and frees all memory used by it. */
void KV_ResetError(void);


/* Returns a null-terminated string with the last set error.
 * This string is always temporary and should *not* be stored by pointer!
 */
const char *KV_GetError(void);


/*********************************************************************************************************************************
 * String printer
 *********************************************************************************************************************************/


struct _KV_Printer {
  char *_buffer;
  size_t _length;
  size_t _expansionstep;

  /* Temporary */
  char *_current;
  size_t _left;
  int _written;
};


/* Initialize a string printer.
 * The initialized printer must be cleared using KV_PrinterClear() when not needed anymore.
 *
 * ctx - String printer to initialize.
 * expansionstep - Amount of bytes to add to the string each time it is expanded and reallocated.
 */
void KV_PrinterInit(KV_Printer *ctx, size_t expansionstep);


/* Clears previously initialized string printer.
 * If the printer buffer was accessed using KV_PrinterGetBuffer() at any point, it becomes invalid.
 */
void KV_PrinterClear(KV_Printer *ctx);


/* Returns current character buffer of a string printer.
 * This string is always temporary and should *not* be stored by pointer!
 *
 * ctx - String printer to retrieve the character buffer from.
 * length - An optional pointer to that will be filled with the returned buffer length afterwards.
 */
char *KV_PrinterGetBuffer(KV_Printer *ctx, size_t *length);


/* Resets the current position in a string printer to the very beginning of the buffer.
 * Any subsequent string printing using KV_PrinterFormat() overwrites previous buffer contents.
 */
void KV_PrinterResetString(KV_Printer *ctx);


/* Format a string and append it at the end of the current character buffer of a string printer.
 * This function internally relies on the standard vsnprintf() function.
 *
 * ctx - String printer to append a formatted string to.
 * format - A null-terminated string specifying how to interpret the data.
 * ... - Arguments specifying data to print.
 */
void KV_PrinterFormat(KV_Printer *ctx, const char *format, ...);


/*********************************************************************************************************************************
 * Parser context
 *********************************************************************************************************************************/


struct _KV_Context {
  /* Input data */
  const char *_directory; /* Directory to read included files from */
  const char *_file; /* File to read from or NULL for reading from character buffers */
  const char *_buffer; /* Beginning of a character buffer to read from */

  /* Length of the content that is being parsed.
   - If reading from a character buffer, specifies maximum buffer size.
     Setting it to -1 makes it parse until a null character in a null-terminated string.
   - If reading from a file, contains file size in bytes only after parsing it at least once, otherwise -1.
  */
  size_t _length;

  /* Flags */
  KV_bool _escapeseq : 1; /* (default: KV_true) Parse escape sequences in strings */
  KV_bool _multikey  : 1; /* (default: KV_true) Allow adding multiple values under the same key */
  KV_bool _overwrite : 1; /* (default: KV_true) Overwrite values of duplicate keys, if '_multikey' is disabled */

  /* Temporary parser data */
  const char *_pch; /* Currently parsed character */
  size_t _line; /* Currently parsed line */
};


/* Setup context for a new character buffer.
 * Each string argument is borrowed for the lifetime of a KV_Context struct instead of copying its content.
 *
 * directory - Where to include files from when using #base and #include macros.
   Empty or non-absolute paths are treated as relative to the current working directory.
 * buffer - Character buffer to read from. May or may not be null-terminated.
 * length - Maximum length of the specified character buffer. If set to -1, reads the buffer until a null character.
 */
void KV_ContextSetupBuffer(KV_Context *ctx, const char *directory, const char *buffer, size_t length);


/* Setup context for a new file.
 * Each string argument is borrowed for the lifetime of a KV_Context struct instead of copying its content.
 *
 * directory - Where to include files from when using #base and #include macros.
   Empty or non-absolute paths are treated as relative to the current working directory.
 * path - Absolute or relative path to a physical file on disk.
 */
void KV_ContextSetupFile(KV_Context *ctx, const char *directory, const char *path);


/* Customize parser behavior by toggling context flags.
 * This function can only be called after KV_ContextSetupBuffer() or KV_ContextSetupFile().
 */
void KV_ContextSetFlags(KV_Context *ctx, KV_bool escapeseq, KV_bool multikey, KV_bool overwrite);


/* Copy context flags from another context.
 */
void KV_ContextCopyFlags(KV_Context *ctx, KV_Context *other);


/*********************************************************************************************************************************
 * One pair of key & value
 *********************************************************************************************************************************/


/* Creates a new key-value pair with an empty list of subpairs.
 * The returned pair must be manually freed using KV_PairDestroy() when not needed anymore.
 *
 * key - Name of this pair or NULL for the root pair.
 */
KV_Pair *KV_NewList(const char *key);


/* Creates a new key-value pair with a string value.
 * The returned pair must be manually freed using KV_PairDestroy() when not needed anymore.
 *
 * key - Name of this pair or NULL for the root pair.
 * value - Null-terminated string in an ANSI encoding.
 */
KV_Pair *KV_NewString(const char *key, const char *value);


/* Creates a new key-value pair with a list of subpairs from another pair.
 * The entire list of subpairs is copied from 'other' and assigned to the new pair.
 * The returned pair must be manually freed using KV_PairDestroy() when not needed anymore.
 *
 * key - Name of this pair or NULL for the root pair.
 * list - Another pair to copy the list of subpairs from.
 */
KV_Pair *KV_NewListFrom(const char *key, KV_Pair *list);


/* Frees all memory used by a pair, including itself and the potential subpairs recursively.
 */
void KV_PairDestroy(KV_Pair *pair);


/* Creates a full copy of an existing key-value pair, including its potential subpairs.
 * The returned pair must be manually freed using KV_PairDestroy() when not needed anymore.
 */
KV_Pair *KV_PairCopy(KV_Pair *pair);


/* Frees memory used by the pair without destroying it.
 * Resets the pair to an empty list of subpairs under a NULL key.
 */
void KV_PairClear(KV_Pair *pair);


/* Sets a new key name to a pair.
 * The key may be NULL, which turns the pair into a "root" one.
 */
void KV_SetKey(KV_Pair *pair, const char *key);


/* Sets a new string value.
 * If the pair was already set up, the previous data is automatically cleared.
 *
 * value - Null-terminated string in an ANSI encoding.
 */
void KV_SetString(KV_Pair *pair, const char *value);


/* Sets a new list of subpairs from another pair.
 * The entire list of subpairs is copied from 'other' and assigned to 'pair'.
 * If the pair was already set up, the previous data is automatically cleared.
 *
 * pair - Pair to assign the list of subpairs to.
 * list - Another pair to copy the list of subpairs from.
 */
void KV_SetListFrom(KV_Pair *pair, KV_Pair *list);


/* Copies the entire list of subpairs from 'other' and appends all of them at the end of 'list'.
 * If 'list' was already set up with a non-list value, the previous data is cleared and replaced with an empty list.
 *
 * list - Pair to copy the list of subpairs into.
 * other - Pair to copy the list of subpairs from.
 * overwrite - Whether to overwrite existing values in 'list' with the new ones from 'other' under the same keys.
 */
void KV_CopyNodes(KV_Pair *list, KV_Pair *other, KV_bool overwrite);


/* Goes through every subpair in 'other' and only adds new subpairs to 'list' that don't exist, recursively for each list.
 * Both pairs must contain a list of subpairs for a value, otherwise it does nothing.
 *
 * list - List to merge all the pairs into.
 * other - List to iterate through.
 * moveNodes - Whether to simply move the nodes over from 'other' to 'list' instead of creating full copies of them.
 */
void KV_MergeNodes(KV_Pair *list, KV_Pair *other, KV_bool moveNodes);


/* Replaces value of 'pair' with the copied value from 'other'.
 * If it's a subpair of another pair, the replacement is also reflected in the parent.
 * The contents of the other pair remain unchanged.
 */
void KV_Replace(KV_Pair *pair, KV_Pair *other);


/* Swaps key names & values between two different pairs, even if either one of them is empty.
 * If either one is a subpair of another pair, the swap is also reflected in the parent.
 */
void KV_Swap(KV_Pair *pair1, KV_Pair *pair2);


/* Prints a formatted pair into a null-terminated character buffer.
 * The returned character buffer must be manually freed when not needed anymore.
 * Returns NULL on error; call KV_GetError() for more information.
 *
 * pair - A pair to print out. If its key is NULL (a root pair), the value is printed as is.
 * length - An optional pointer to that will be filled with the returned buffer length afterwards.
 * expansionstep - Amount of bytes to add to the string each time it is expanded and reallocated.
 * indentation - A string to use for indenting each nested subpair, e.g. one tab character.
 */
char *KV_Print(KV_Pair *pair, size_t *length, size_t expansionstep, const char *indentation);


/*********************************************************************************************************************************
 * Doubly linked lists
 *********************************************************************************************************************************/


/* Checks whether a list has any subpairs.
 * If the pair value isn't a list, always returns KV_false.
 */
KV_bool KV_HasNodes(KV_Pair *list);


/* Returns amount of subpairs in a list.
 * If the pair value isn't a list, always returns -1.
 */
size_t KV_GetNodeCount(KV_Pair *list);


/* Returns n-th subpair from a list (if n >= 0) or NULL (if n >= KV_GetNodeCount()).
 * If the pair value isn't a list, always returns NULL.
 */
KV_Pair *KV_GetPair(KV_Pair *list, size_t n);


/* Returns the first subpair under the specified key, otherwise NULL.
 * If the pair value isn't a list, always returns NULL.
 */
KV_Pair *KV_FindPair(KV_Pair *list, const char *key);


/* Returns the first subpair of a specific type under the specified key, otherwise NULL.
 * If the pair value isn't a list, always returns NULL.
 */
KV_Pair *KV_FindPairOfType(KV_Pair *list, const char *key, KV_DataType type);


/* Check if a pair under the specified key is empty.
 * Returns KV_true if the pair isn't found or if the found pair has no subpairs in a list.
 */
KV_bool KV_IsEmpty(KV_Pair *list, const char *key);


/* Returns a string from the first subpair under the specified key or 'defaultValue' if not found.
 */
const char *KV_FindString(KV_Pair *list, const char *key, const char *defaultValue);


/* Returns the first subpair from a pair.
 * If the pair value isn't a list, always returns NULL.
 */
KV_Pair *KV_GetHead(KV_Pair *list);


/* Returns the last subpair from a pair.
 * If the pair value isn't a list, always returns NULL.
 */
KV_Pair *KV_GetTail(KV_Pair *list);


/* Prepends a new subpair at the beginning of a list.
 * If the pair value isn't a list, it does nothing.
 * KV_PairDestroy() should *not* be called on 'other' after this because it's not copied!
 */
void KV_AddHead(KV_Pair *list, KV_Pair *other);


/* Appends a new subpair at the end of a list.
 * If the pair value isn't a list, it does nothing.
 * KV_PairDestroy() should *not* be called on 'other' after this because it's not copied!
 */
void KV_AddTail(KV_Pair *list, KV_Pair *other);


/* Insert 'pair' node before 'other' node, which may be in the middle of some list. */
void KV_InsertBefore(KV_Pair *pair, KV_Pair *other);


/* Insert 'pair' node after 'other' node, which may be in the middle of some list. */
void KV_InsertAfter(KV_Pair *pair, KV_Pair *other);


/* Expunge a node from whatever list it's currently in. */
void KV_Expunge(KV_Pair *pair);


/* Returns the previous neighbor of a subpair. */
KV_Pair *KV_GetPrev(KV_Pair *pair);


/* Returns the next neighbor of a subpair. */
KV_Pair *KV_GetNext(KV_Pair *pair);


/*********************************************************************************************************************************
 * Pair values
 * NOTE: These functions *do not* perform any safety checks and may lead to undefined behavior if not used properly!
 *********************************************************************************************************************************/


/* Returns the key name of a pair. */
char *KV_GetKey(KV_Pair *pair);


/* Returns data type of a pair value. */
KV_DataType KV_GetDataType(KV_Pair *pair);


/* Returns the string value of a pair. */
char *KV_GetString(KV_Pair *pair);


/*********************************************************************************************************************************
 * Serialization
 *********************************************************************************************************************************/


/* Constructs a list of subpairs by parsing within certain context.
 * Returns NULL on error; call KV_GetError() for more information.
 */
KV_Pair *KV_Parse(KV_Context *ctx);


/* Constructs a list of subpairs by directly parsing a character array in the current working directory.
 * Returns NULL on error; call KV_GetError() for more information.
 *
 * buffer - Character buffer to read from. May or may not be null-terminated.
 * length - Maximum length of the specified character buffer. If set to -1, reads the buffer until a null character.
 */
KV_Pair *KV_ParseBuffer(const char *buffer, size_t length);


/* Constructs a list of subpairs by directly parsing a file in the current working directory.
 * Returns NULL on error; call KV_GetError() for more information.
 *
 * path - Absolute or relative path to a physical file on disk.
 */
KV_Pair *KV_ParseFile(const char *path);


/* Save contents of a pair into a file.
 * Returns KV_false on error; call KV_GetError() for more information.
 *
 * path - Absolute or relative path to a physical file on disk.
 */
KV_bool KV_Save(KV_Pair *pair, const char *path);


#ifdef __cplusplus
}
#endif

#endif /* VDF_KEYVALUES_INCL_H */
