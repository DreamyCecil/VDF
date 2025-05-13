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
#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

/* MSVC wackness */
#ifdef _MSC_VER
  #pragma warning(disable : 4996)
  #define _CRT_SECURE_NO_WARNINGS

  #if _MSC_VER < 1900
    #define snprintf _snprintf
  #endif

  #define strncasecmp _strnicmp
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "keyvalues.h"

/* Check if it's a full path string */
KV_INLINE KV_bool IsPathStringAbsolute(const char *str) {
#ifdef _WIN32
  /* Should have at least 3 non-null characters */
  if (!str[0] || !str[1] || !str[2]) return KV_false;
  /* Starts with a drive letter, a colon and a path separator, e.g. "C:/" */
  return ((str[2] == '/' || str[2] == '\\') && isalpha((unsigned char)str[0]) && str[1] == ':') ? KV_true : KV_false;

#else
  /* Starts with a path separator, e.g. "/home" */
  return (str[0] == '/' || str[0] == '\\') ? KV_true : KV_false;
#endif
};

/*********************************************************************************************************************************
 * Key-value types
 *********************************************************************************************************************************/

struct _KV_List {
  /* If there's only one subpair, both pointers reference the same pair */
  KV_Pair *_head; /* First pair in the list */
  KV_Pair *_tail; /* Last pair in the list */
};

struct _KV_Pair {
  char *_key; /* Name of the key */
  KV_DataType _type; /* Data type of a stored value */

  union {
    char *str; /* A single value as a string */
    KV_List *list; /* An array of key-value pairs */
  } _value;

  KV_List *_parent; /* List that owns this subpair in a chain */
  KV_Pair *_prev; /* Previous neighboring pair or NULL for the head */
  KV_Pair *_next; /* Next neighboring pair or NULL for the tail */
};

typedef struct _KV_PrintContext {
  char *_buffer;
  size_t _length;
  size_t _expansionstep;

  /* Temporary */
  char *_current;
  size_t _left;
} KV_PrintContext;

/*********************************************************************************************************************************
 * Error handling
 *********************************************************************************************************************************/

static char *_strError = NULL;
static int _iErrorSet = 0; /* 0 - no; 1 - proper error; 2 - errno */

KV_INLINE void KV_ResetError(void) {
  if (_strError && _iErrorSet == 1) {
    KV_free(_strError);
  }

  _strError = NULL;
  _iErrorSet = 0;
};

/* Set new last error message.
 * If 'ctx' is non-NULL, prepends the error message with the current context line.
 */
KV_INLINE void KV_SetError(KV_Context *ctx, const char *str) {
  size_t ctLen;

  /* Clear previous proper error */
  if (_strError && _iErrorSet == 1) {
    KV_free(_strError);
  }

  /* Extra characters for extra text + null terminator */
  ctLen = strlen(str) + 64;
  _strError = (char *)KV_malloc(ctLen);

  if (_strError) {
    /* Within parser context */
    if (ctx) {
      sprintf(_strError, "Line %ld : %s", ctx->_line, str);

    /* Generic error */
    } else {
      strcpy(_strError, str);
    }
    _iErrorSet = 1;

  /* malloc() failure */
  } else {
    _strError = strerror(errno);
    _iErrorSet = 2;
  }
};

const char *KV_GetError(void) {
  return (_iErrorSet ? _strError : "No error");
};

/*********************************************************************************************************************************
 * Parser context
 *********************************************************************************************************************************/

void KV_ContextSetupBuffer(KV_Context *ctx, const char *directory, const char *buffer, size_t length) {
  ctx->_directory = directory;
  ctx->_file = NULL;
  ctx->_buffer = buffer;
  ctx->_length = length;

  ctx->_pch = buffer;
  ctx->_line = 1;

  KV_ContextSetFlags(ctx, KV_true, KV_false, KV_true);
};

void KV_ContextSetupFile(KV_Context *ctx, const char *directory, const char *path) {
  ctx->_directory = directory;
  ctx->_file = path;
  ctx->_buffer = NULL;
  ctx->_length = (size_t)-1;

  ctx->_pch = NULL;
  ctx->_line = 0;

  KV_ContextSetFlags(ctx, KV_true, KV_false, KV_true);
};

void KV_ContextSetFlags(KV_Context *ctx, KV_bool escapeseq, KV_bool multikey, KV_bool overwrite) {
  ctx->_escapeseq = escapeseq;
  ctx->_multikey  = multikey;
  ctx->_overwrite = overwrite;
};

void KV_ContextCopyFlags(KV_Context *ctx, KV_Context *other) {
  ctx->_escapeseq = other->_escapeseq;
  ctx->_multikey  = other->_multikey;
  ctx->_overwrite = other->_overwrite;
};

/* Check if the character buffer reached the end */
KV_INLINE KV_bool KV_ContextBufferEnded(KV_Context *ctx) {
  /* Reached a null character */
  if (ctx->_length == (size_t)-1 && !*ctx->_pch) return KV_true;

  /* Reached the maximum length */
  return ((size_t)(ctx->_pch - ctx->_buffer) >= ctx->_length) ? KV_true : KV_false;
};

KV_INLINE void KV_PrintContextSetup(KV_PrintContext *ctx, size_t step) {
  ctx->_left = ctx->_length = ctx->_expansionstep = step;

  ctx->_buffer = (char *)KV_malloc(ctx->_length);
  ctx->_current = ctx->_buffer;
};

KV_INLINE KV_bool KV_PrintContextNeedToExpand(KV_PrintContext *ctx, int iWritten) {
  size_t iOffset;

  /* String has been written correctly, no need to expand */
  if (iWritten >= 0 && (size_t)iWritten < ctx->_left)
  {
    ctx->_current += iWritten;
    ctx->_left -= iWritten;
    return KV_false;
  }

  /* Expand the buffer */
  iOffset = ctx->_current - ctx->_buffer;

  ctx->_length += ctx->_expansionstep;
  ctx->_buffer = (char *)KV_realloc(ctx->_buffer, ctx->_length);

  ctx->_current = ctx->_buffer + iOffset;
  ctx->_left = ctx->_length - iOffset;

  return KV_true;
};

/*********************************************************************************************************************************
 * Lists of key-value pairs
 *********************************************************************************************************************************/

KV_List *KV_NewList(void) {
  /* Allocate the list and reset its state */
  KV_List *list = (KV_List *)KV_malloc(sizeof(KV_List));

  list->_head = list->_tail = NULL;

  return list;
};

void KV_ListDestroy(KV_List *list) {
  /* Clear the list and free it */
  KV_ListClear(list);
  KV_free(list);
};

KV_List *KV_ListCopy(KV_List *other) {
  KV_Pair *pair;
  KV_List *list = (KV_List *)KV_malloc(sizeof(KV_List));

  list->_head = list->_tail = NULL;

  /* Iterate through the pairs and add their copies to this list */
  for (pair = other->_head; pair; pair = pair->_next)
  {
    KV_ListAddTail(list, KV_PairCopy(pair));
  }

  return list;
};

void KV_ListClear(KV_List *list) {
  KV_Pair *pairDestroy;
  KV_Pair *pair = list->_head;

  /* Destroy all pairs */
  while (pair) {
    pairDestroy = pair;
    pair = pair->_next;

    KV_PairDestroy(pairDestroy);
  }

  /* Reset list state */
  list->_head = list->_tail = NULL;
};

static KV_bool KV_PairPrintInternal(KV_Pair *pair, KV_PrintContext *ctx, size_t depth, const char *indentation);

static KV_bool KV_ListPrintInternal(KV_List *list, KV_PrintContext *ctx, size_t depth, const char *indentation) {
  KV_Pair *pair;

  /* Print each pair in the list */
  for (pair = list->_head; pair; pair = pair->_next)
  {
    if (!KV_PairPrintInternal(pair, ctx, depth, indentation)) {
      return KV_false;
    }
  }

  return KV_true;
};

char *KV_ListPrint(KV_List *list, size_t *length, size_t expansionstep, const char *indentation) {
  KV_PrintContext ctx;
  KV_PrintContextSetup(&ctx, expansionstep);

  if (KV_ListPrintInternal(list, &ctx, 0, indentation))
  {
    if (length) *length = ctx._length;
    return ctx._buffer;
  }

  /* Free buffer on error */
  KV_free(ctx._buffer);
  return NULL;
};

/*********************************************************************************************************************************
 * One pair of key & value
 *********************************************************************************************************************************/

KV_Pair *KV_NewPair(void) {
  /* Allocate the pair and reset its state */
  KV_Pair *pair = (KV_Pair *)KV_malloc(sizeof(KV_Pair));

  pair->_key = NULL;
  pair->_type = KV_TYPE_NONE;
  pair->_value.list = KV_NewList();

  pair->_parent = NULL;
  pair->_prev = pair->_next = NULL;

  return pair;
};

KV_Pair *KV_NewPairString(const char *key, const char *value) {
  /* Allocate the pair and set new values */
  KV_Pair *pair = (KV_Pair *)KV_malloc(sizeof(KV_Pair));

  pair->_key = KV_strdup(key);
  pair->_type = KV_TYPE_STRING;
  pair->_value.str = KV_strdup(value);

  pair->_parent = NULL;
  pair->_prev = pair->_next = NULL;

  return pair;
};

KV_Pair *KV_NewPairList(const char *key, KV_List *list) {
  /* Allocate the pair and set new values */
  KV_Pair *pair = (KV_Pair *)KV_malloc(sizeof(KV_Pair));

  pair->_key = KV_strdup(key);
  pair->_type = KV_TYPE_NONE;
  pair->_value.list = list;

  pair->_parent = NULL;
  pair->_prev = pair->_next = NULL;

  return pair;
};

/* Free all memory associated with the pair without resetting any fields */
KV_INLINE void KV_PairFreeMemory(KV_Pair *pair) {
  /* Destroy key */
  if (pair->_key) {
    KV_free(pair->_key);
  }

  /* Destroy value */
  switch (pair->_type) {
    case KV_TYPE_NONE:
      KV_ListDestroy(pair->_value.list);
      break;

    case KV_TYPE_STRING:
      KV_free(pair->_value.str);
      break;

    default: break;
  }
};

void KV_PairDestroy(KV_Pair *pair) {
  /* Unlink the pair */
  KV_PairExpunge(pair);

  /* Clear the pair and free it */
  KV_PairFreeMemory(pair);
  KV_free(pair);
};

KV_Pair *KV_PairCopy(KV_Pair *other) {
  KV_Pair *pair = (KV_Pair *)KV_malloc(sizeof(KV_Pair));

  pair->_key = KV_strdup(other->_key);
  pair->_type = other->_type;

  switch (pair->_type) {
    case KV_TYPE_NONE:
      pair->_value.list = KV_ListCopy(other->_value.list);
      break;

    case KV_TYPE_STRING:
      pair->_value.str = KV_strdup(other->_value.str);
      break;

    default:
      pair->_type = KV_TYPE_NONE;
      pair->_value.list = KV_NewList();
      break;
  }

  pair->_parent = NULL;
  pair->_prev = pair->_next = NULL;

  return pair;
};

void KV_PairClear(KV_Pair *pair) {
  /* Free all memory */
  KV_PairFreeMemory(pair);

  /* Reset the pair state but preserve the neighboring connections */
  pair->_key = NULL;
  pair->_type = KV_TYPE_NONE;
  pair->_value.list = KV_NewList();
};

void KV_PairSetString(KV_Pair *pair, const char *key, const char *value) {
  /* Clear last pair before setting a new one */
  KV_PairFreeMemory(pair);

  pair->_key = KV_strdup(key);
  pair->_type = KV_TYPE_STRING;
  pair->_value.str = KV_strdup(value);
};

void KV_PairSetList(KV_Pair *pair, const char *key, KV_List *list) {
  /* Clear last pair before setting a new one */
  KV_PairFreeMemory(pair);

  pair->_key = KV_strdup(key);
  pair->_type = KV_TYPE_NONE;
  pair->_value.list = list;
};

void KV_PairReplace(KV_Pair *pair, KV_Pair *other) {
  /* Clear last pair before setting a new one */
  KV_PairFreeMemory(pair);

  pair->_key = KV_strdup(other->_key);
  pair->_type = other->_type;

  switch (pair->_type) {
    case KV_TYPE_NONE:
      pair->_value.list = KV_ListCopy(other->_value.list);
      break;

    case KV_TYPE_STRING:
      pair->_value.str = KV_strdup(other->_value.str);
      break;

    default:
      pair->_type = KV_TYPE_NONE;
      pair->_value.list = KV_NewList();
      break;
  }
};

void KV_PairSwap(KV_Pair *pair1, KV_Pair *pair2) {
  KV_Pair temp;

  KV_List *parent1, *parent2;
  KV_Pair *prev1, *prev2;
  KV_Pair *next1, *next2;

  /* Remember the neighbors */
  parent1 = pair1->_parent;
  prev1 = pair1->_prev;
  next1 = pair1->_next;

  parent2 = pair2->_parent;
  prev2 = pair2->_prev;
  next2 = pair2->_next;

  /* Swap the values */
  temp = *pair1;
  *pair1 = *pair2;
  *pair2 = temp;

  /* Restore the neighbors */
  pair1->_parent = parent1;
  pair1->_prev = prev1;
  pair1->_next = next1;

  pair2->_parent = parent2;
  pair2->_prev = prev2;
  pair2->_next = next2;
};

/* IMPORTANT: Returned pointer needs to be manually freed! */
KV_INLINE char *KV_ConvertEscapeSeq(char *pch) {
  char *str;
  size_t i;

  str = (char *)KV_calloc(strlen(pch) * 2 + 1, sizeof(char));
  i = 0;

  while (*pch) {
    switch (*pch) {
      case '\n': str[i++] = '\\'; str[i++] = 'n';  break;
      case '\t': str[i++] = '\\'; str[i++] = 't';  break;
      case '\r': str[i++] = '\\'; str[i++] = 'r';  break;
      case '\b': str[i++] = '\\'; str[i++] = 'b';  break;
      case '\f': str[i++] = '\\'; str[i++] = 'f';  break;
      case '"':  str[i++] = '\\'; str[i++] = '"';  break;
      case '\\': str[i++] = '\\'; str[i++] = '\\'; break;
      default: str[i++] = *pch; break;
    }

    ++pch;
  }

  return str;
};

KV_bool KV_PairPrintInternal(KV_Pair *pair, KV_PrintContext *ctx, size_t depth, const char *indentation) {
  char *strIdent;
  size_t ct;
  int iWritten;
  char *strValue;

  strIdent = (char *)KV_calloc(depth * strlen(indentation) + 1, sizeof(char));
  ct = depth;

  while (ct --> 0) {
    strcat(strIdent, indentation);
  }

/* Convenience macros that keep reallocating the buffer until the printed string fits */
#define PRINT1(_Format, _Arg1) \
  do { \
    iWritten = snprintf(ctx->_current, ctx->_left, _Format, _Arg1); \
  } while (KV_PrintContextNeedToExpand(ctx, iWritten));

#define PRINT2(_Format, _Arg1, _Arg2) \
  do { \
    iWritten = snprintf(ctx->_current, ctx->_left, _Format, _Arg1, _Arg2); \
  } while (KV_PrintContextNeedToExpand(ctx, iWritten));

  /* Print a key */
  PRINT2("%s\"%s\"", strIdent, pair->_key);

  /* Print a value */
  switch (pair->_type) {
    case KV_TYPE_NONE: {
      PRINT1("\n%s{\n", strIdent);

      if (!KV_ListPrintInternal(pair->_value.list, ctx, depth + 1, indentation)) {
        KV_free(strIdent);
        return KV_false;
      }

      PRINT1("%s}\n", strIdent);

      KV_free(strIdent);
    } return KV_true;

    case KV_TYPE_STRING: {
      strValue = KV_ConvertEscapeSeq(pair->_value.str);
      PRINT2("%s\"%s\"\n", indentation, strValue);

      KV_free(strValue);
      KV_free(strIdent);
    } return KV_true;

    /* Exit the switch */
    default: break;
  }

  /* Unknown value type */
  KV_SetError(NULL, "Unknown value type");
  KV_free(strIdent);
  return KV_false;

#undef PRINT1
#undef PRINT2
};

char *KV_PairPrint(KV_Pair *pair, size_t *length, size_t expansionstep, const char *indentation) {
  KV_PrintContext ctx;
  KV_PrintContextSetup(&ctx, expansionstep);

  if (KV_PairPrintInternal(pair, &ctx, 0, indentation))
  {
    if (length) *length = ctx._length;
    return ctx._buffer;
  }

  /* Free buffer on error */
  KV_free(ctx._buffer);
  return NULL;
};

/*********************************************************************************************************************************
 * Doubly linked lists
 *********************************************************************************************************************************/

KV_bool KV_IsListEmpty(KV_List *list) {
  return list->_head ? KV_false : KV_true;
};

size_t KV_ListCount(KV_List *list) {
  KV_Pair *pair;
  size_t ct = 0;

  pair = list->_head;

  while (pair) {
    pair = pair->_next;
    ++ct;
  }

  return ct;
};

KV_Pair *KV_GetPair(KV_List *list, size_t n) {
  KV_Pair *pair;

  for (pair = list->_head; pair; pair = pair->_next)
  {
    if (n == 0) return pair;
    --n;
  }

  return NULL;
};

KV_Pair *KV_FindPair(KV_List *list, const char *key) {
  KV_Pair *pair;

  for (pair = list->_head; pair; pair = pair->_next)
  {
    if (!strcmp(pair->_key, key)) return pair;
  }

  return NULL;
};

KV_Pair *KV_FindPairOfType(KV_List *list, const char *key, KV_DataType type) {
  KV_Pair *pair;

  for (pair = list->_head; pair; pair = pair->_next)
  {
    if (pair->_type != type) continue;

    if (!strcmp(pair->_key, key)) return pair;
  }

  return NULL;
};

char *KV_FindString(KV_List *list, const char *key) {
  KV_Pair *pair;

  for (pair = list->_head; pair; pair = pair->_next)
  {
    if (pair->_type != KV_TYPE_STRING) continue;

    if (!strcmp(pair->_key, key)) return pair->_value.str;
  }

  return NULL;
};

KV_List *KV_FindList(KV_List *list, const char *key) {
  KV_Pair *pair;

  for (pair = list->_head; pair; pair = pair->_next)
  {
    if (pair->_type != KV_TYPE_NONE) continue;

    if (!strcmp(pair->_key, key)) return pair->_value.list;
  }

  return NULL;
};

KV_Pair *KV_GetHead(KV_List *list) {
  return list->_head;
};

KV_Pair *KV_GetTail(KV_List *list) {
  return list->_tail;
};

/* Setup the very first pair in some list's chain */
KV_INLINE void KV_ListSetFirstPair(KV_List *list, KV_Pair *pair) {
  /* This is the first pair in a chain */
  list->_head = pair;
  list->_tail = pair;

  /* Relink the pair to this chain */
  KV_PairExpunge(pair);
  pair->_parent = list;
};

void KV_ListAddHead(KV_List *list, KV_Pair *pair) {
  /* Insert at the beginning if there is already a chain */
  if (list->_head) {
    KV_PairInsertBefore(pair, list->_head);
  } else {
    KV_ListSetFirstPair(list, pair);
  }
};

void KV_ListAddTail(KV_List *list, KV_Pair *pair) {
  /* Insert at the end if there is already a chain */
  if (list->_tail) {
    KV_PairInsertAfter(pair, list->_tail);
  } else {
    KV_ListSetFirstPair(list, pair);
  }
};

/* Unlink a pair from its previous neighbor */
KV_INLINE void KV_PairUnlinkPrev(KV_Pair *pair) {
  if (pair->_prev != NULL) pair->_prev->_next = NULL;
  pair->_prev = NULL;
};

/* Unlink a pair from its next neighbor */
KV_INLINE void KV_PairUnlinkNext(KV_Pair *pair) {
  if (pair->_next != NULL) pair->_next->_prev = NULL;
  pair->_next = NULL;
};

void KV_PairInsertBefore(KV_Pair *pair, KV_Pair *other) {
  KV_Pair *before;

  /* Remove from the current chain and borrow the new parent */
  KV_PairExpunge(pair);
  pair->_parent = other->_parent;

  /* Relink the parent to this new node */
  if (other->_parent->_head == other) {
    other->_parent->_head = pair;
  }

  /* Remember the node that goes before this one (may be NULL) */
  before = other->_prev;
  KV_PairUnlinkPrev(other);

  /* Link the other node after this one */
  other->_prev = pair;
  pair->_next = other;

  /* Link the node that goes before */
  if (before) {
    pair->_prev = before;
    before->_next = pair;
  }
};

void KV_PairInsertAfter(KV_Pair *pair, KV_Pair *other) {
  KV_Pair *after;

  /* Remove from the current chain and borrow the new parent */
  KV_PairExpunge(pair);
  pair->_parent = other->_parent;

  /* Relink the parent to this new node */
  if (other->_parent->_tail == other) {
    other->_parent->_tail = pair;
  }

  /* Remember the node that goes after this one (may be NULL) */
  after = other->_next;
  KV_PairUnlinkNext(other);

  /* Link the other node before this one */
  other->_next = pair;
  pair->_prev = other;

  /* Link the node that goes after */
  if (after) {
    pair->_next = after;
    after->_prev = pair;
  }
};

void KV_PairExpunge(KV_Pair *pair) {
  /* Link neighboring pairs together */
  if (pair->_prev) pair->_prev->_next = pair->_next;
  if (pair->_next) pair->_next->_prev = pair->_prev;

  /* Relink list head and tail */
  if (pair->_parent) {
    if (pair->_parent->_head == pair) {
      pair->_parent->_head = pair->_next;
    }

    if (pair->_parent->_tail == pair) {
      pair->_parent->_tail = pair->_prev;
    }
  }

  /* Reset the links */
  pair->_prev = pair->_next = NULL;
};

KV_Pair *KV_GetPrev(KV_Pair *pair) {
  return pair->_prev;
};

KV_Pair *KV_GetNext(KV_Pair *pair) {
  return pair->_next;
};

char *KV_GetKey(KV_Pair *pair) {
  return pair->_key;
};

KV_DataType KV_GetDataType(KV_Pair *pair) {
  return pair->_type;
};

char *KV_GetString(KV_Pair *pair) {
  return pair->_value.str;
};

KV_List *KV_GetList(KV_Pair *pair) {
  return pair->_value.list;
};

/*********************************************************************************************************************************
 * Serialization
 *********************************************************************************************************************************/

static KV_List *KV_ParseBufferInternal(KV_Context *ctx, KV_bool inner);

KV_INLINE KV_List *KV_ParseFileInternal(KV_Context *ctx) {
  FILE *file;
  char *str;
  KV_List *list;
  KV_Context ctxParse;

  /* Disregard the directory if it's an empty string or the file path is absolute */
  if (!*ctx->_directory || IsPathStringAbsolute(ctx->_file)) {
    file = fopen(ctx->_file, "rb");
    
  /* Otherwise compose a full path to the file */
  } else {
    str = (char *)KV_malloc(strlen(ctx->_directory) + strlen(ctx->_file) + 1);
    strcpy(str, ctx->_directory);
    strcat(str, ctx->_file);

    file = fopen(str, "rb");
    KV_free(str);
  }

  if (!file) {
    str = (char *)KV_malloc(strlen(strerror(errno)) + 22);
    strcpy(str, "Cannot include file: ");
    strcat(str, strerror(errno));

    KV_SetError(ctx, str);
    KV_free(str);
    return NULL;
  }

  /* Get file size */
  fseek(file, 0, SEEK_END);
  ctx->_length = ftell(file);
  fseek(file, 0, SEEK_SET);

  /* Read file contents into the string and close the file */
  str = (char *)KV_malloc(ctx->_length);
  fread(str, sizeof(char), ctx->_length, file);
  fclose(file);

  /* Parse file contents and then free them */
  KV_ContextSetupBuffer(&ctxParse, ctx->_directory, str, ctx->_length);
  KV_ContextCopyFlags(&ctxParse, ctx);

  list = KV_ParseBufferInternal(&ctxParse, KV_false);
  KV_free(str);

  return list;
};

/* IMPORTANT: Returned pointer needs to be manually freed! */
KV_INLINE char *KV_ParseString(KV_Context *ctx, KV_bool onlyquotes)
{
  char *str;
  size_t ctCapacity, iChar;

  ctCapacity = 256;
  iChar = 0;
  str = (char *)KV_malloc(ctCapacity + 1);

  for (;;) {
    /* Quit the loop on specific characters */
    if (onlyquotes) {
      if (*ctx->_pch == '"') break;
    } else {
      if (*ctx->_pch == '"' || *ctx->_pch == '/' || *ctx->_pch == '{' || *ctx->_pch == '}' || isspace(*ctx->_pch)) break;
    }

    /* Unexpected end of the string */
    if (KV_ContextBufferEnded(ctx) || *ctx->_pch == '\n') {
      KV_SetError(ctx, "Unclosed string");
      KV_free(str);
      return NULL;
    }

    /* Allocate more space for the string */
    if (iChar >= ctCapacity) {
      ctCapacity += 256;
      str = (char *)KV_realloc(str, ctCapacity);
    }

    /* Parse escape sequence */
    if (ctx->_escapeseq && *ctx->_pch == '\\') {
      ++ctx->_pch;

      if (KV_ContextBufferEnded(ctx)) {
        KV_SetError(ctx, "Unclosed string");
        KV_free(str);
        return NULL;
      }

      /* Append special character */
      switch (*ctx->_pch) {
        case 'n':  str[iChar++] = '\n'; break;
        case 't':  str[iChar++] = '\t'; break;
        case 'r':  str[iChar++] = '\r'; break;
        case 'b':  str[iChar++] = '\b'; break;
        case 'f':  str[iChar++] = '\f'; break;
        case '"':  str[iChar++] = '"';  break;
        case '\\': str[iChar++] = '\\'; break;
        default: break;
      }

    /* Append a regular character */
    } else {
      str[iChar++] = *ctx->_pch;
    }

    ++ctx->_pch;
  }

  /* Skip closing quotes */
  ++ctx->_pch;

  /* Terminate the string */
  str[iChar] = '\0';

  return str;
};

KV_INLINE KV_bool KV_ParseJunkInternal(KV_Context *ctx) {
  /* Count line breaks */
  if (*ctx->_pch == '\n') {
    ++ctx->_pch;
    ++ctx->_line;
    return KV_true;
  }

  /* Comments: Ignore all characters until a line break */
  if (*ctx->_pch == '/') {
    ++ctx->_pch;

    while (!KV_ContextBufferEnded(ctx) && *ctx->_pch != '\n') {
      ++ctx->_pch;
    }

    return KV_true;
  }

  /* Skip whitespaces */
  if (isspace(*ctx->_pch)) {
    ++ctx->_pch;
    return KV_true;
  }

  return KV_false;
};

KV_List *KV_ParseBufferInternal(KV_Context *ctx, KV_bool inner) {
  KV_List *list;
  char *strKey;
  KV_List *listTemp;
  KV_Pair *pairIter;
  KV_Pair *pairFind;
  KV_Context ctxInclude;

  const char *pchCheck;
  char *strTemp;

  list = KV_NewList();
  strKey = NULL; /* Set to a string if expecting a value string for a complete pair */

  while (!KV_ContextBufferEnded(ctx)) {
    /* Parse unimportant junk */
    if (KV_ParseJunkInternal(ctx)) continue;

    pchCheck = ctx->_pch++;

    /* Lists: Parse another list between curly braces */
    if (strKey && *pchCheck == '{') {
      listTemp = KV_ParseBufferInternal(ctx, KV_true);

      /* Couldn't parse an inner list */
      if (!listTemp) {
        KV_ListDestroy(list);
        return NULL;
      }

      /* Catch duplicate keys */
      if (!ctx->_multikey && (pairFind = KV_FindPair(list, strKey))) {
        if (ctx->_overwrite) {
          KV_PairSetList(pairFind, strKey, listTemp);

          KV_free(strKey);
          strKey = NULL;
          continue;
        }

        KV_SetError(ctx, "Key already exists");
        KV_free(strKey);
        KV_ListDestroy(listTemp);
        KV_ListDestroy(list);
        return NULL;
      }

      KV_ListAddTail(list, KV_NewPairList(strKey, listTemp));

      KV_free(strKey);
      strKey = NULL;
      continue;
    }

    /* List end, if not expecting a value */
    if (inner && !strKey && *pchCheck == '}') return list;

    /* Strings: Parse all characters until another double quote */
    if (*pchCheck == '"') {
      strTemp = KV_ParseString(ctx, KV_true);

    /* Strings: Parse all characters until another token or whitespace */
    } else {
      --ctx->_pch;
      strTemp = KV_ParseString(ctx, KV_false);
    }

    /* Couldn't parse a string token */
    if (!strTemp) {
      KV_ListDestroy(list);
      return NULL;
    }

    /* Remember this key string for future use */
    if (!strKey) {
      strKey = strTemp;

      /* Pair setup takes ownership of the string, so don't free it */
      strTemp = NULL;
      continue;
    }

    /* Include macros */
    if (!strncasecmp(strKey, "#base", 5) || !strncasecmp(strKey, "#include", 8)) {
      /* Get the list from a file */
      KV_ContextSetupFile(&ctxInclude, ctx->_directory, strTemp);
      KV_ContextCopyFlags(&ctxInclude, ctx);

      listTemp = KV_ParseFileInternal(&ctxInclude);

      /* Free strings after macro execution */
      KV_free(strKey);
      KV_free(strTemp);
      strKey = NULL;
      strTemp = NULL;

      /* Couldn't parse an included list */
      if (!listTemp) {
        KV_ListDestroy(list);
        return NULL;
      }

      /* Append all pairs to the current list */
      for (pairIter = listTemp->_head; pairIter; pairIter = pairIter->_next)
      {
        strTemp = pairIter->_key;

        /* Catch duplicate keys */
        if (!ctx->_multikey && (pairFind = KV_FindPair(list, strTemp))) {
          if (ctx->_overwrite) {
            KV_PairSwap(pairFind, pairIter);
            continue;
          }

          KV_SetError(ctx, "Key already exists");
          KV_ListDestroy(listTemp);
          KV_ListDestroy(list);
          return NULL;
        }

        /* Append a copy of a pair */
        KV_ListAddTail(list, KV_PairCopy(pairIter));
      }

      strTemp = NULL;
      KV_ListDestroy(listTemp);
      continue;
    }

    /* Catch duplicate keys */
    if (!ctx->_multikey && (pairFind = KV_FindPair(list, strKey))) {
      if (ctx->_overwrite) {
        KV_PairSetString(pairFind, strKey, strTemp);

        KV_free(strKey);
        KV_free(strTemp);
        strKey = NULL;
        strTemp = NULL;
        continue;
      }

      KV_SetError(ctx, "Key already exists");
      KV_free(strKey);
      KV_free(strTemp);
      KV_ListDestroy(list);
      return NULL;
    }

    /* Add new key-value pair */
    KV_ListAddTail(list, KV_NewPairString(strKey, strTemp));

    KV_free(strKey);
    KV_free(strTemp);
    strKey = NULL;
    strTemp = NULL;
  }

  return list;
};

KV_List *KV_Parse(KV_Context *ctx) {
  if (ctx->_file) {
    return KV_ParseFileInternal(ctx);
  }

  return KV_ParseBufferInternal(ctx, KV_false);
};

KV_List *KV_ParseBuffer(const char *buffer, size_t length) {
  KV_Context ctx;
  KV_ContextSetupBuffer(&ctx, "", buffer, length);

  return KV_ParseBufferInternal(&ctx, KV_false);
};

KV_List *KV_ParseFile(const char *path) {
  KV_Context ctx;
  KV_ContextSetupFile(&ctx, "", path);

  return KV_ParseFileInternal(&ctx);
};

KV_bool KV_Save(KV_List *list, const char *path) {
  FILE *file;
  char *str;

  if (!list || !path) {
    KV_SetError(NULL, "No list or path specified");
    return KV_false;
  }

  file = fopen(path, "w");

  if (!file) {
    str = (char *)KV_malloc(strlen(strerror(errno)) + 31);
    strcpy(str, "Cannot open file for writing: ");
    strcat(str, strerror(errno));

    KV_SetError(NULL, str);
    KV_free(str);
    return KV_false;
  }

  str = KV_ListPrint(list, NULL, 1048576, "\t"); /* 1MB step */

  if (!str) {
    fclose(file);
    return KV_false;
  }

  fputs(str, file);
  KV_free(str);

  fclose(file);
  return KV_true;
};
