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

#ifdef __STDC_VERSION__
  #define KV_INLINE static inline
#else
  #define KV_INLINE static
#endif


/* You can redefine this functions to provide your own means of memory management */
#if defined(KV_malloc) || defined(KV_calloc) || defined(KV_realloc) || defined(KV_free)
  /* Make sure all memory management functions are redefined */
  #ifndef KV_malloc
    #error Please define memory allocation function for 'KV_malloc'
  #endif
  #ifndef KV_calloc
    #error Please define memory allocation function with nullifying for 'KV_calloc'
  #endif
  #ifndef KV_realloc
    #error Please define memory reallocation function for 'KV_realloc'
  #endif
  #ifndef KV_free
    #error Please define memory freeing function for 'KV_free'
  #endif

#else
  /* Use default memory management functions */
  #include <stdlib.h>

  #ifndef KV_malloc
    #define KV_malloc malloc
  #endif
  #ifndef KV_calloc
    #define KV_calloc calloc
  #endif
  #ifndef KV_realloc
    #define KV_realloc realloc
  #endif
  #ifndef KV_free
    #define KV_free free
  #endif
#endif


/*********************************************************************************************************************************
 * Key-value types
 *********************************************************************************************************************************/


/* Boolean type */
typedef enum _KV_bool {
  KV_false = 0,
  KV_true  = 1,
} KV_bool;


typedef struct _KV_Context KV_Context; /* Parser context for reading VDF contents */
typedef struct _KV_List KV_List; /* An array of key-value pairs */
typedef struct _KV_Pair KV_Pair; /* A string/list value under some key */


/*********************************************************************************************************************************
 * Error handling
 *********************************************************************************************************************************/


/* Returns a null-terminated string with the last set error.
 * This string is always temporary and should *not* be stored by pointer!
 */
const char *KV_GetError(void);


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
  KV_bool _multikey  : 1; /* (default: KV_false) Allow adding multiple values under the same key */
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
 * Lists of key-value pairs
 *********************************************************************************************************************************/


/* Creates a new key-value list by dynamically allocating memory for it and resets it to the default state.
 * The returned pointer list must be manually freed using KV_ListDestroy() when not needed anymore.
 */
KV_List *KV_NewList(void);


/* Frees all memory used by a previously created key-value list, recursively for all list values.
 */
void KV_ListDestroy(KV_List *list);


/* Creates a full copy of an existing key-value list by recursively allocating memory for each of its elements.
 * The returned list must be manually freed using KV_ListDestroy() when not needed anymore.
 */
KV_List *KV_ListCopy(KV_List *list);


/* Frees memory of all list pairs without destroying the list itself and resets the list to an empty state.
 */
void KV_ListClear(KV_List *list);


/* Adds a new pair to the end of the list by taking ownership of the provided pointer.
 * KV_PairDestroy() should *not* be called on 'pair' after this!
 */
void KV_ListAppend(KV_List *list, KV_Pair *pair);


/* Prints a formatted list of key-value pairs into a null-terminated character buffer.
 * The returned character buffer must be manually freed when not needed anymore.
 * Returns NULL on error; call KV_GetError() for more information.
 *
 * length - An optional pointer to that will be filled with the returned buffer length afterwards.
 * expansionstep - Amount of bytes to add to the string each time it is expanded and reallocated.
 * indentation -  A string to use for indenting pairs of each nested list value, e.g. one tab character.
 */
char *KV_ListPrint(KV_List *list, size_t *length, size_t expansionstep, const char *indentation);


/*********************************************************************************************************************************
 * One pair of key & value
 *********************************************************************************************************************************/


/* Creates a new key-value pair by dynamically allocating memory for it and resets it to the default state.
 * The returned pair must be manually freed using KV_PairDestroy() when not needed anymore.
 */
KV_Pair *KV_NewPair(void);


/* Creates a new key-value pair with a key name and a string value by taking ownership of them.
 * It is functionally identical to calling KV_PairNew() and then KV_PairSetString().
 * 'key' and 'value' should *not* be freed after this!
 */
KV_Pair *KV_NewPairString(char *key, char *value);


/* Creates a new key-value pair with a key name and a list value by taking ownership of them.
 * It is functionally identical to calling KV_PairNew() and then KV_PairSetList().
 * 'key' should *not* be freed and KV_ListDestroy() should *not* be called on 'list' after this!
 */
KV_Pair *KV_NewPairList(char *key, KV_List *list);


/* Creates a new key-value pair with a key name and a string value.
 * It is functionally identical to calling KV_NewPairString() with strdup() around the key and value strings.
 */
KV_Pair *KV_NewPairStringDup(const char *key, const char *value);


/* Creates a new key-value pair with a key name and a list value.
 * It is functionally identical to calling KV_NewPairList() with strdup() around the key string.
 * KV_ListDestroy() should *not* be called on 'list' after this!
 */
KV_Pair *KV_NewPairListDup(const char *key, KV_List *list);


/* Frees all memory used by a previously created key-value pair, recursively for all list values.
 */
void KV_PairDestroy(KV_Pair *pair);


/* Creates a full copy of an existing key-value pair by recursively allocating memory for its value.
 * The returned pair must be manually freed using KV_PairDestroy() when not needed anymore.
 */
KV_Pair *KV_PairCopy(KV_Pair *pair);


/* Frees memory of the pair without destroying the list itself and resets the pair to an empty state.
 */
void KV_PairClear(KV_Pair *pair);


/* Sets a new key-value pair by taking ownership of the key and value strings.
 * If the pair was already set up, it is automatically cleared using KV_PairClear().
 * 'key' and 'value' should *not* be freed after this!
 */
void KV_PairSetString(KV_Pair *pair, char *key, char *value);


/* Sets a new key-value pair by taking ownership of the key string and the list value.
 * If the pair was already set up, it is automatically cleared using KV_PairClear().
 * 'key' should *not* be freed and KV_ListDestroy() should *not* be called on 'list' after this!
 */
void KV_PairSetList(KV_Pair *pair, char *key, KV_List *list);


/* Replace contents of one pair with the contents from another pair by making a full copy of them.
 * If the first pair is inside a list, the replacement will also be reflected in that list.
 * The contents of the other pair remain unchanged.
 */
void KV_PairReplace(KV_Pair *pair, KV_Pair *other);


/* Swap values between two different pairs, even if either one of them is empty.
 * If either pair is inside a list, the swap will also be reflected in that list.
 */
void KV_PairSwap(KV_Pair *pair1, KV_Pair *pair2);


/* Prints a formatted key-value pair into a null-terminated character buffer.
 * The returned character buffer must be manually freed when not needed anymore.
 * Returns NULL on error; call KV_GetError() for more information.
 *
 * length - An optional pointer to that will be filled with the returned buffer length afterwards.
 * expansionstep - Amount of bytes to add to the string each time it is expanded and reallocated.
 * indentation - A string to use for indenting pairs of each nested list value, e.g. one tab character.
 */
char *KV_PairPrint(KV_Pair *pair, size_t *length, size_t expansionstep, const char *indentation);


/*********************************************************************************************************************************
 * Value access
 * NOTE: These functions *do not* perform any safety checks and may lead to undefined behavior if not used properly!
 *********************************************************************************************************************************/


/* Checks whether a list has no pairs in it. */
KV_bool KV_IsListEmpty(KV_List *list);


/* Returns amount of pairs in a list. */
size_t KV_ListCount(KV_List *list);


/* Returns array of pairs from a list.
 * Can be used for sorting purposes along with KV_ListCount(), e.g. via qsort()
 */
KV_Pair **KV_ListArray(KV_List *list);


/* Returns a pair under the specified array index from a list, otherwise NULL.
 * Can be used for iterating through the entire array until it returns the first NULL.
 */
KV_Pair *KV_GetPair(KV_List *list, size_t i);


/* Returns the first pair under the specified key from a list, otherwise NULL. */
KV_Pair *KV_FindPair(KV_List *list, const char *key);


/* Returns a string from the first pair under the specified key from a list, otherwise NULL. */
char *KV_FindString(KV_List *list, const char *key);


/* Returns a list from the first pair under the specified key from a list, otherwise NULL. */
KV_List *KV_FindList(KV_List *list, const char *key);


/* Checks whether a pair is set up. */
KV_bool KV_IsPairEmpty(KV_Pair *pair);


/* Returns the key name of a pair. */
char *KV_GetKey(KV_Pair *pair);


/* Checks whether a pair has a list value. */
KV_bool KV_HasListValue(KV_Pair *pair);


/* Returns the string value of a pair. */
char *KV_GetString(KV_Pair *pair);


/* Returns the list value of a pair. */
KV_List *KV_GetList(KV_Pair *pair);


/*********************************************************************************************************************************
 * Serialization
 *********************************************************************************************************************************/


/* Constructs a list of key-values by parsing within certain context.
 * Returns NULL on error; call KV_GetError() for more information.
 */
KV_List *KV_Parse(KV_Context *ctx);


/* Constructs a list of key-values by directly parsing a character array in the current working directory.
 * Returns NULL on error; call KV_GetError() for more information.
 *
 * buffer - Character buffer to read from. May or may not be null-terminated.
 * length - Maximum length of the specified character buffer. If set to -1, reads the buffer until a null character.
 */
KV_List *KV_ParseBuffer(const char *buffer, size_t length);


/* Constructs a list of key-values by directly parsing a file in the current working directory.
 * Returns NULL on error; call KV_GetError() for more information.
 *
 * path - Absolute or relative path to a physical file on disk.
 */
KV_List *KV_ParseFile(const char *path);


/* Save contents of an existing list into a file.
 * Returns KV_false on error; call KV_GetError() for more information.
 *
 * path - Absolute or relative path to a physical file on disk.
 */
KV_bool KV_Save(KV_List *list, const char *path);


#ifdef __cplusplus
}
#endif

#endif /* VDF_KEYVALUES_INCL_H */
