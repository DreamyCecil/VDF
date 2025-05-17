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
#include <assert.h>

#include "keyvalues.h"

#ifdef VDF_MANAGE_MEMORY
  void *(*KV_malloc)(size_t bytes)                = malloc;
  void *(*KV_calloc)(size_t ct, size_t elemSize)  = calloc;
  void *(*KV_realloc)(void *memory, size_t bytes) = realloc;
  void  (*KV_free)(void *memory)                  = free;
  char *(*KV_strdup)(const char *str)             = strdup;
#endif

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

struct _KV_Pair {
  char *_key; /* Name of the key (NULL for a root pair) */
  KV_DataType _type; /* Data type of a stored value */

  union {
    char *str; /* A single value as a string */

    /* A list of subpairs */
    struct {
      /* If there's only one subpair, both pointers reference the same one */
      KV_Pair *head;
      KV_Pair *tail;
    };
  } _value;

  KV_Pair *_parent; /* Pair that owns this subpair in a list */
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

void KV_ResetError(void) {
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
  char *strLastError;
  size_t ctLen;

  assert(str);

  /* Remember the last proper error to free it at the end */
  strLastError = (_iErrorSet == 1) ? _strError : NULL;

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

  /* Free previous error string */
  if (strLastError) KV_free(strLastError);
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

  KV_ContextSetFlags(ctx, KV_true, KV_true, KV_true);
};

void KV_ContextSetupFile(KV_Context *ctx, const char *directory, const char *path) {
  ctx->_directory = directory;
  ctx->_file = path;
  ctx->_buffer = NULL;
  ctx->_length = (size_t)-1;

  ctx->_pch = NULL;
  ctx->_line = 0;

  KV_ContextSetFlags(ctx, KV_true, KV_true, KV_true);
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
 * One pair of key & value
 *********************************************************************************************************************************/

KV_Pair *KV_NewList(const char *key) {
  /* Allocate the pair and reset its state */
  KV_Pair *pair = (KV_Pair *)KV_malloc(sizeof(KV_Pair));

  pair->_key = (key ? KV_strdup(key) : NULL);
  pair->_type = KV_TYPE_NONE;
  pair->_value.head = pair->_value.tail = NULL;

  pair->_parent = NULL;
  pair->_prev = pair->_next = NULL;

  return pair;
};

KV_Pair *KV_NewString(const char *key, const char *value) {
  /* Allocate the pair and set new values */
  KV_Pair *pair = (KV_Pair *)KV_malloc(sizeof(KV_Pair));

  assert(value);

  pair->_key = (key ? KV_strdup(key) : NULL);
  pair->_type = KV_TYPE_STRING;
  pair->_value.str = KV_strdup(value);

  pair->_parent = NULL;
  pair->_prev = pair->_next = NULL;

  return pair;
};

KV_Pair *KV_NewListFrom(const char *key, KV_Pair *list) {
  /* Allocate the pair and set new values */
  KV_Pair *pair = (KV_Pair *)KV_malloc(sizeof(KV_Pair));

  assert(list);

  pair->_key = (key ? KV_strdup(key) : NULL);
  pair->_type = KV_TYPE_NONE;
  pair->_value.head = pair->_value.tail = NULL;
  KV_CopyNodes(pair, list, KV_false);

  pair->_parent = NULL;
  pair->_prev = pair->_next = NULL;

  return pair;
};

/* Free memory of the pair key without resetting the field */
KV_INLINE void KV_FreeKey(KV_Pair *pair) {
  if (pair->_key) KV_free(pair->_key);
};

/* Free all memory associated with the pair value without resetting any fields */
KV_INLINE void KV_FreeValue(KV_Pair *pair) {
  KV_Pair *pairDestroy;
  KV_Pair *pairIter;

  /* Destroy value */
  switch (pair->_type) {
    case KV_TYPE_NONE:
      pairIter = pair->_value.head;

      /* Destroy all pairs */
      while (pairIter) {
        pairDestroy = pairIter;
        pairIter = pairIter->_next;

        KV_PairDestroy(pairDestroy);
      }
      break;

    case KV_TYPE_STRING:
      KV_free(pair->_value.str);
      break;

    default:
      assert(!"Unknown value type");
      break;
  }
};

void KV_PairDestroy(KV_Pair *pair) {
  assert(pair);

  /* Unlink the pair */
  KV_Expunge(pair);

  /* Clear the pair and free it */
  KV_FreeKey(pair);
  KV_FreeValue(pair);
  KV_free(pair);
};

KV_Pair *KV_PairCopy(KV_Pair *other) {
  KV_Pair *pair = (KV_Pair *)KV_malloc(sizeof(KV_Pair));

  assert(other);

  pair->_key = KV_strdup(other->_key);
  pair->_type = other->_type;

  switch (pair->_type) {
    case KV_TYPE_NONE:
      pair->_value.head = pair->_value.tail = NULL;
      KV_CopyNodes(pair, other, KV_false);
      break;

    case KV_TYPE_STRING:
      pair->_value.str = KV_strdup(other->_value.str);
      break;

    default:
      assert(!"Unknown value type");
      pair->_type = KV_TYPE_NONE;
      pair->_value.head = pair->_value.tail = NULL;
      break;
  }

  pair->_parent = NULL;
  pair->_prev = pair->_next = NULL;

  return pair;
};

void KV_PairClear(KV_Pair *pair) {
  assert(pair);

  /* Free all memory */
  KV_FreeKey(pair);
  KV_FreeValue(pair);

  /* Reset the pair state but preserve the neighboring connections */
  pair->_key = NULL;
  pair->_type = KV_TYPE_NONE;
  pair->_value.head = pair->_value.tail = NULL;
};

void KV_SetKey(KV_Pair *pair, const char *key) {
  char *keyCopy;

  assert(pair);

  /* Root pair */
  if (!key) {
    KV_FreeKey(pair);
    pair->_key = NULL;
    return;
  }

  /* Copy the string beforehand in case it is the same */
  keyCopy = KV_strdup(key);

  KV_FreeKey(pair);
  pair->_key = keyCopy;
};

void KV_SetString(KV_Pair *pair, const char *value) {
  char *valueCopy;

  assert(pair && value);

  /* Copy the string beforehand in case it is the same, otherwise the data is wiped before KV_strdup() */
  valueCopy = KV_strdup(value);

  /* Clear last pair before setting a new one */
  KV_FreeValue(pair);

  pair->_type = KV_TYPE_STRING;
  pair->_value.str = valueCopy;
};

void KV_SetListFrom(KV_Pair *pair, KV_Pair *list) {
  assert(pair && list);

  /* Clear last pair before setting a new one */
  KV_FreeValue(pair);

  pair->_type = KV_TYPE_NONE;
  pair->_value.head = pair->_value.tail = NULL;
  KV_CopyNodes(pair, list, KV_false);
};

void KV_CopyNodes(KV_Pair *list, KV_Pair *other, KV_bool overwrite) {
  KV_Pair *pairIter, *pairFind;
  assert(list && other);

  /* Set an entirely new list if the current value isn't a list */
  if (list->_type != KV_TYPE_NONE) {
    KV_FreeValue(list);

    list->_type = KV_TYPE_NONE;
    list->_value.head = list->_value.tail = NULL;
  }

  /* Add copies of all subpairs to this list */
  for (pairIter = other->_value.head; pairIter; pairIter = pairIter->_next)
  {
    /* Replace duplicate keys */
    if (overwrite && (pairFind = KV_FindPair(list, pairIter->_key))) {
      KV_Replace(pairFind, pairIter);
      continue;
    }

    KV_AddTail(list, KV_PairCopy(pairIter));
  }
};

void KV_MergeNodes(KV_Pair *list, KV_Pair *other, KV_bool moveNodes) {
  KV_Pair *pairIter, *pairFind;
  assert(list && other);

  /* Both must be lists */
  if (list->_type != KV_TYPE_NONE || other->_type != KV_TYPE_NONE) return;

  /* Add copies of non-existent subpairs to this list */
  pairIter = other->_value.head;

  while (pairIter) {
    /* Recursively merge existing subpairs */
    if ((pairFind = KV_FindPair(list, pairIter->_key))) {
      KV_MergeNodes(pairFind, pairIter, moveNodes);

      /* Get the next subpair */
      pairIter = pairIter->_next;
      continue;
    }

    /* Remember the current subpair and get the next one */
    pairFind = pairIter;
    pairIter = pairIter->_next;

    /* Move that subpair over to the current list instead of copying it */
    if (moveNodes) {
      KV_AddTail(list, pairFind);
    } else {
      KV_AddTail(list, KV_PairCopy(pairFind));
    }
  }
};

void KV_Replace(KV_Pair *pair, KV_Pair *other) {
  assert(pair && other);
  if (pair == other) return;

  /* Clear last value before setting a new one */
  KV_FreeValue(pair);

  pair->_type = other->_type;

  switch (pair->_type) {
    case KV_TYPE_NONE:
      pair->_value.head = pair->_value.tail = NULL;
      KV_CopyNodes(pair, other, KV_false);
      break;

    case KV_TYPE_STRING:
      pair->_value.str = KV_strdup(other->_value.str);
      break;

    default:
      assert(!"Unknown value type");
      pair->_type = KV_TYPE_NONE;
      pair->_value.head = pair->_value.tail = NULL;
      break;
  }
};

/* Swap all struct data between two different pairs */
KV_INLINE void KV_SwapWholePairs(KV_Pair *pair1, KV_Pair *pair2) {
  KV_Pair temp = *pair1;
  *pair1 = *pair2;
  *pair2 = temp;
};

void KV_Swap(KV_Pair *pair1, KV_Pair *pair2) {
  KV_Pair *parent1, *parent2;
  KV_Pair *prev1, *prev2;
  KV_Pair *next1, *next2;

  assert(pair1 && pair2);
  if (pair1 == pair2) return;

  /* Remember the neighbors */
  parent1 = pair1->_parent;
  prev1 = pair1->_prev;
  next1 = pair1->_next;

  parent2 = pair2->_parent;
  prev2 = pair2->_prev;
  next2 = pair2->_next;

  /* Swap the values */
  KV_SwapWholePairs(pair1, pair2);

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

static KV_bool KV_PrintInternal(KV_Pair *pair, KV_PrintContext *ctx, size_t depth, const char *indentation) {
  KV_bool bValueAfterKey;
  char *strIndent;
  size_t ct;
  int iWritten;
  KV_Pair *pairIter;
  char *strValue;

  assert(pair);

  bValueAfterKey = (pair->_key ? KV_true : KV_false);

  /* If this is a subpair */
  if (bValueAfterKey) {
    /* Create extra indentation */
    strIndent = (char *)KV_calloc(depth * strlen(indentation) + 1, sizeof(char));
    ct = depth;

    while (ct --> 0) {
      strcat(strIndent, indentation);
    }

    /* Advance the depth */
    ++depth;

  /* If there's no key and it's a list (a root pair) */
  } else if (pair->_type == KV_TYPE_NONE) {
    /* Create empty indentation for subpairs */
    strIndent = (char *)KV_calloc(1, sizeof(char));

  /* Otherwise the value without key cannot be printed */
  } else {
    KV_SetError(NULL, "Subpair has no key");
    return KV_false;
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
  if (bValueAfterKey) PRINT2("%s\"%s\"", strIndent, pair->_key);

  /* Print a value */
  switch (pair->_type) {
    case KV_TYPE_NONE: {
      if (bValueAfterKey) PRINT1("\n%s{\n", strIndent);

      /* Print each pair in the list */
      for (pairIter = pair->_value.head; pairIter; pairIter = pairIter->_next)
      {
        if (!KV_PrintInternal(pairIter, ctx, depth, indentation)) {
          KV_free(strIndent);
          return KV_false;
        }
      }

      if (bValueAfterKey) PRINT1("%s}\n", strIndent);

      KV_free(strIndent);
    } return KV_true;

    case KV_TYPE_STRING: {
      strValue = KV_ConvertEscapeSeq(pair->_value.str);

      if (bValueAfterKey) {
        PRINT2("%s\"%s\"\n", indentation, strValue);
      } else {
        PRINT1("\"%s\"\n", strValue);
      }

      KV_free(strValue);
      KV_free(strIndent);
    } return KV_true;

    /* Exit the switch */
    default: break;
  }

  /* Unknown value type */
  assert(!"Unknown value type");

  KV_SetError(NULL, "Unknown value type");
  KV_free(strIndent);
  return KV_false;

#undef PRINT1
#undef PRINT2
};

char *KV_Print(KV_Pair *pair, size_t *length, size_t expansionstep, const char *indentation) {
  KV_PrintContext ctx;
  KV_PrintContextSetup(&ctx, expansionstep);

  if (KV_PrintInternal(pair, &ctx, 0, indentation))
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

KV_bool KV_HasNodes(KV_Pair *list) {
  /* Not a list */
  assert(list);
  if (list->_type != KV_TYPE_NONE) return KV_false;

  return list->_value.head ? KV_true : KV_false;
};

size_t KV_GetNodeCount(KV_Pair *list) {
  size_t ct = 0;

  /* Not a list */
  assert(list);
  assert(list->_type == KV_TYPE_NONE);
  if (list->_type != KV_TYPE_NONE) return (size_t)(-1);

  list = list->_value.head;

  while (list) {
    list = list->_next;
    ++ct;
  }

  return ct;
};

KV_Pair *KV_GetPair(KV_Pair *list, size_t n) {
  /* Not a list */
  assert(list);
  assert(list->_type == KV_TYPE_NONE);
  if (list->_type != KV_TYPE_NONE) return NULL;

  for (list = list->_value.head; list; list = list->_next)
  {
    if (n == 0) return list;
    --n;
  }

  return NULL;
};

KV_Pair *KV_FindPair(KV_Pair *list, const char *key) {
  /* Not a list */
  assert(list);
  assert(list->_type == KV_TYPE_NONE);
  if (list->_type != KV_TYPE_NONE) return NULL;

  for (list = list->_value.head; list; list = list->_next)
  {
    if (!strcmp(list->_key, key)) return list;
  }

  return NULL;
};

KV_Pair *KV_FindPairOfType(KV_Pair *list, const char *key, KV_DataType type) {
  /* Not a list */
  assert(list);
  assert(list->_type == KV_TYPE_NONE);
  if (list->_type != KV_TYPE_NONE) return NULL;

  for (list = list->_value.head; list; list = list->_next)
  {
    if (list->_type != type) continue;

    if (!strcmp(list->_key, key)) return list;
  }

  return NULL;
};

KV_bool KV_IsEmpty(KV_Pair *list, const char *key) {
  KV_Pair *pair = KV_FindPair(list, key);

  /* No pair found */
  if (!pair) return KV_true;

  /* Has a value */
  if (pair->_type != KV_TYPE_NONE) return KV_false;

  /* Has no subpairs */
  return list->_value.head ? KV_true : KV_false;
};

const char *KV_FindString(KV_Pair *list, const char *key, const char *defaultValue) {
  KV_Pair *pair = KV_FindPairOfType(list, key, KV_TYPE_STRING);
  return (pair ? pair->_value.str : defaultValue);
};

KV_Pair *KV_GetHead(KV_Pair *list) {
  /* Not a list */
  assert(list);
  assert(list->_type == KV_TYPE_NONE);
  if (list->_type != KV_TYPE_NONE) return NULL;

  return list->_value.head;
};

KV_Pair *KV_GetTail(KV_Pair *list) {
  /* Not a list */
  assert(list);
  assert(list->_type == KV_TYPE_NONE);
  if (list->_type != KV_TYPE_NONE) return NULL;

  return list->_value.tail;
};

/* Setup the very first pair in a list */
KV_INLINE void KV_SetFirstPair(KV_Pair *pair, KV_Pair *first) {
  pair->_value.head = pair->_value.tail = first;

  /* Relink the pair to this list */
  KV_Expunge(first);
  first->_parent = pair;
};

void KV_AddHead(KV_Pair *list, KV_Pair *other) {
  /* Not a list */
  assert(list);
  assert(list->_type == KV_TYPE_NONE);
  if (list->_type != KV_TYPE_NONE) return;

  /* Insert at the beginning if there is already a list */
  if (list->_value.head) {
    KV_InsertBefore(other, list->_value.head);
  } else {
    KV_SetFirstPair(list, other);
  }
};

void KV_AddTail(KV_Pair *list, KV_Pair *other) {
  /* Not a list */
  assert(list);
  assert(list->_type == KV_TYPE_NONE);
  if (list->_type != KV_TYPE_NONE) return;

  /* Insert at the end if there is already a list */
  if (list->_value.tail) {
    KV_InsertAfter(other, list->_value.tail);
  } else {
    KV_SetFirstPair(list, other);
  }
};

/* Unlink a pair from its previous neighbor */
KV_INLINE void KV_UnlinkPrev(KV_Pair *pair) {
  if (pair->_prev != NULL) pair->_prev->_next = NULL;
  pair->_prev = NULL;
};

/* Unlink a pair from its next neighbor */
KV_INLINE void KV_UnlinkNext(KV_Pair *pair) {
  if (pair->_next != NULL) pair->_next->_prev = NULL;
  pair->_next = NULL;
};

void KV_InsertBefore(KV_Pair *pair, KV_Pair *other) {
  KV_Pair *before;
  assert(pair && other);

  /* Remove from the current list and borrow the new parent */
  KV_Expunge(pair);
  pair->_parent = other->_parent;

  /* Relink the parent to this new node */
  if (other->_parent->_value.head == other) {
    other->_parent->_value.head = pair;
  }

  /* Remember the node that goes before this one (may be NULL) */
  before = other->_prev;
  KV_UnlinkPrev(other);

  /* Link the other node after this one */
  other->_prev = pair;
  pair->_next = other;

  /* Link the node that goes before */
  if (before) {
    pair->_prev = before;
    before->_next = pair;
  }
};

void KV_InsertAfter(KV_Pair *pair, KV_Pair *other) {
  KV_Pair *after;
  assert(pair && other);

  /* Remove from the current list and borrow the new parent */
  KV_Expunge(pair);
  pair->_parent = other->_parent;

  /* Relink the parent to this new node */
  if (other->_parent->_value.tail == other) {
    other->_parent->_value.tail = pair;
  }

  /* Remember the node that goes after this one (may be NULL) */
  after = other->_next;
  KV_UnlinkNext(other);

  /* Link the other node before this one */
  other->_next = pair;
  pair->_prev = other;

  /* Link the node that goes after */
  if (after) {
    pair->_next = after;
    after->_prev = pair;
  }
};

void KV_Expunge(KV_Pair *pair) {
  assert(pair);

  /* Link neighboring pairs together */
  if (pair->_prev) pair->_prev->_next = pair->_next;
  if (pair->_next) pair->_next->_prev = pair->_prev;

  /* Relink list head and tail */
  if (pair->_parent) {
    if (pair->_parent->_value.head == pair) {
      pair->_parent->_value.head = pair->_next;
    }

    if (pair->_parent->_value.tail == pair) {
      pair->_parent->_value.tail = pair->_prev;
    }
  }

  /* Reset the links */
  pair->_prev = pair->_next = NULL;
};

KV_Pair *KV_GetPrev(KV_Pair *pair) {
  assert(pair);
  return pair->_prev;
};

KV_Pair *KV_GetNext(KV_Pair *pair) {
  assert(pair);
  return pair->_next;
};

/*********************************************************************************************************************************
 * Pair values
 *********************************************************************************************************************************/

char *KV_GetKey(KV_Pair *pair) {
  assert(pair);
  return pair->_key;
};

KV_DataType KV_GetDataType(KV_Pair *pair) {
  assert(pair);
  return pair->_type;
};

char *KV_GetString(KV_Pair *pair) {
  assert(pair);
  assert(pair->_type == KV_TYPE_STRING);
  return pair->_value.str;
};

/*********************************************************************************************************************************
 * Serialization
 *********************************************************************************************************************************/

static KV_Pair *KV_ParseBufferInternal(KV_Context *ctx, KV_bool inner);

KV_INLINE KV_Pair *KV_ParseFileInternal(KV_Context *ctx) {
  FILE *file;
  char *str;
  KV_Pair *list;
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
    if (!onlyquotes) {
      if (*ctx->_pch == '"' || *ctx->_pch == '/' || *ctx->_pch == '{' || *ctx->_pch == '}' || isspace(*ctx->_pch)) {
        break;
      }

    /* Skip closing quotes */
    } else if (*ctx->_pch == '"') {
      ++ctx->_pch;
      break;
    }

    /* Unexpected end of the string */
    if (KV_ContextBufferEnded(ctx) || *ctx->_pch == '\n') {
      /* Fine with unquoted strings */
      if (!onlyquotes) break;

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

      /* Insert a single backslash, if at the very end */
      if (KV_ContextBufferEnded(ctx)) {
        str[iChar++] = '\\';
        break;
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

  /* Terminate the string */
  str[iChar] = '\0';

  return str;
};

/* Count line breaks */
KV_INLINE KV_bool KV_ParseLineBreak(KV_Context *ctx)
{
  if (*ctx->_pch == '\n') {
    ++ctx->_pch;
    ++ctx->_line;
    return KV_true;
  }

  return KV_false;
};

/* Comments: Ignore all characters in CPP-styled single-line comments or in C-styled block comments */
/* NOTE: Single '/' characters with no '/' or '*' afterwards count as "empty" comments and are simply ignored */
KV_INLINE KV_bool KV_ParseComments(KV_Context *ctx)
{
  /* Not a comment */
  if (*ctx->_pch != '/') return KV_false;

  ++ctx->_pch;
  if (KV_ContextBufferEnded(ctx)) return KV_true;

  /* C++ comments */
  if (*ctx->_pch == '/') {
    ++ctx->_pch;

    /* Expect a line break down the road */
    while (!KV_ContextBufferEnded(ctx) && *ctx->_pch != '\n') {
      ++ctx->_pch;
    }

  /* C comments */
  } else if (*ctx->_pch == '*') {
    ++ctx->_pch;

    /* Expect block comment closing down the road */
    while (!KV_ContextBufferEnded(ctx)) {
      /* Keep counting line breaks */
      if (KV_ParseLineBreak(ctx)) continue;

      if (*ctx->_pch == '*') {
        ++ctx->_pch;

        /* Close the block comment or pretend that unclosed ones are still comments */
        if (KV_ContextBufferEnded(ctx) || *ctx->_pch == '/') break;
      }

      ++ctx->_pch;
    }
  }

  /* Ignored enough characters */
  return KV_true;
};

KV_INLINE KV_Pair *KV_IncludeFile(KV_Context *ctx, const char *strFile) {
  /* Get the list from a file */
  KV_Context ctxInclude;
  KV_ContextSetupFile(&ctxInclude, ctx->_directory, strFile);
  KV_ContextCopyFlags(&ctxInclude, ctx);

  return KV_ParseFileInternal(&ctxInclude);
};

KV_INLINE KV_bool KV_AppendIncludedPairs(KV_Context *ctx, KV_Pair *list, KV_Pair *listInclude) {
  KV_Pair *pairIter, *pairFind;

  /* Append all pairs to the current list */
  pairIter = listInclude->_value.head;

  while (pairIter) {
    /* Catch duplicate keys */
    if (!ctx->_multikey && (pairFind = KV_FindPair(list, pairIter->_key))) {
      /* Overwrite values under the same key */
      if (ctx->_overwrite) {
        KV_Swap(pairFind, pairIter);

        /* Get the next subpair */
        pairIter = pairIter->_next;
        continue;
      }

      /* Or throw an error */
      KV_SetError(ctx, "Key already exists");
      return KV_false;
    }

    /* Remember the current subpair and get the next one */
    pairFind = pairIter;
    pairIter = pairIter->_next;

    /* Move that subpair over to the current list instead of copying it */
    KV_AddTail(list, pairFind);
  }

  return KV_true;
};

KV_INLINE KV_bool KV_MergeBasePairs(KV_Context *ctx, KV_Pair *list, KV_Pair *listInclude) {
  /* Moving nodes over from the temporary list to the current list */
  KV_MergeNodes(list, listInclude, KV_true);
  return KV_true;
};

KV_INLINE KV_bool KV_ParseInnerList(KV_Context *ctx, KV_Pair *list, const char *strKey) {
  KV_Pair *pairFind;
  KV_Pair *listTemp = KV_ParseBufferInternal(ctx, KV_true);

  /* Couldn't parse an inner list */
  if (!listTemp) return KV_false;

  /* Catch duplicate keys */
  if (!ctx->_multikey && (pairFind = KV_FindPair(list, strKey))) {
    /* Overwrite values under the same key */
    if (ctx->_overwrite) {
      /* Swap the found pair with this temporary list */
      KV_SetKey(listTemp, strKey);
      KV_Swap(pairFind, listTemp);

      /* Temporary list now contains the found pair data, which isn't needed anymore */
      KV_PairDestroy(listTemp);
      return KV_true;
    }

    /* Or throw an error */
    KV_SetError(ctx, "Key already exists");

    KV_PairDestroy(listTemp);
    return KV_false;
  }

  /* Append a new (or a duplicate) list */
  KV_SetKey(listTemp, strKey);
  KV_AddTail(list, listTemp);

  return KV_true;
};

KV_INLINE KV_bool KV_AddStringPair(KV_Context *ctx, KV_Pair *list, const char *strKey, const char *strValue) {
  KV_Pair *pairFind;

  /* Catch duplicate keys */
  if (!ctx->_multikey && (pairFind = KV_FindPair(list, strKey))) {
    /* Overwrite values under the same key */
    if (ctx->_overwrite) {
      KV_SetString(pairFind, strValue);
      return KV_true;
    }

    /* Or throw an error */
    KV_SetError(ctx, "Key already exists");
    return KV_false;
  }

  /* Append a new (or a duplicate) pair */
  KV_AddTail(list, KV_NewString(strKey, strValue));
  return KV_true;
};

/* Expanding array of lists to include in the current list */
typedef struct _KV_Includes {
  KV_Pair **aLists;
  size_t ctArray;
  size_t ctUsed;
} KV_Includes;

KV_INLINE void KV_InitIncludes(KV_Includes *incl) {
  incl->aLists = NULL;
  incl->ctArray = incl->ctUsed = 0;
};

KV_INLINE void KV_DestroyIncludes(KV_Includes *incl) {
  size_t i;

  if (!incl->aLists) return;

  /* Destroy all lists */
  for (i = 0; i < incl->ctUsed; ++i) {
    KV_PairDestroy(incl->aLists[i]);
  }

  /* Free the array */
  KV_free(incl->aLists);
};

KV_INLINE void KV_AddInclude(KV_Includes *incl, KV_Pair *list) {
  assert(incl->ctUsed <= incl->ctArray);

  /* If all array slots have been used up */
  if (incl->ctUsed == incl->ctArray) {
    incl->ctArray += 32;

    /* Expand the array */
    if (incl->aLists) {
      incl->aLists = (KV_Pair **)KV_realloc(incl->aLists, incl->ctArray * sizeof(KV_Pair *));

    /* Allocate a new array */
    } else {
      incl->aLists = (KV_Pair **)KV_malloc(incl->ctArray * sizeof(KV_Pair *));
    }
  }

  /* Add a new list at the end */
  incl->aLists[incl->ctUsed++] = list;
};

KV_Pair *KV_ParseBufferInternal(KV_Context *ctx, KV_bool inner) {
  KV_Pair *list;
  const char *pchCheck;

  char *strTemp;
  char *strKey;

  KV_Includes inclIncludeFiles, inclBaseFiles;
  KV_Pair *listInclude;
  size_t iInclude;

  list = KV_NewList(NULL);
  strKey = NULL; /* Set to a valid string if expecting a value for a complete pair */

  KV_InitIncludes(&inclIncludeFiles);
  KV_InitIncludes(&inclBaseFiles);

  while (!KV_ContextBufferEnded(ctx)) {
    /* Parse line breaks and comments */
    if (KV_ParseLineBreak(ctx)) continue;
    if (KV_ParseComments(ctx)) continue;

    /* Skip whitespaces */
    if (isspace(*ctx->_pch)) {
      ++ctx->_pch;
      continue;
    }

    pchCheck = ctx->_pch++;

    /* Lists: Parse another list between curly braces */
    if (strKey && *pchCheck == '{') {
      /* Parsed an inner list under some key */
      if (KV_ParseInnerList(ctx, list, strKey)) {
        KV_free(strKey);
        strKey = NULL;
        continue;
      }

      /* Or errored out */
      KV_free(strKey);

      KV_DestroyIncludes(&inclIncludeFiles);
      KV_DestroyIncludes(&inclBaseFiles);
      KV_PairDestroy(list);
      return NULL;
    }

    /* List end, if not expecting a value */
    if (inner && !strKey && *pchCheck == '}') break;

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
      /* Free remembered key string */
      if (strKey) KV_free(strKey);

      KV_DestroyIncludes(&inclIncludeFiles);
      KV_DestroyIncludes(&inclBaseFiles);
      KV_PairDestroy(list);
      return NULL;
    }

    /* Remember this key string for future use */
    if (!strKey) {
      strKey = strTemp;
      continue;
    }

    /* Include macros */
    if (!strncasecmp(strKey, "#include", 8)) {
      /* Added pairs from the included list */
      listInclude = KV_IncludeFile(ctx, strTemp);

      /* Free strings after macro execution */
      KV_free(strKey);
      KV_free(strTemp);
      strKey = NULL;

      if (listInclude) {
        KV_AddInclude(&inclIncludeFiles, listInclude);
        continue;
      }

      /* Or errored out */
      KV_DestroyIncludes(&inclIncludeFiles);
      KV_DestroyIncludes(&inclBaseFiles);
      KV_PairDestroy(list);
      return NULL;

    } else if (!strncasecmp(strKey, "#base", 5)) {
      /* Added pairs from the included list */
      listInclude = KV_IncludeFile(ctx, strTemp);

      /* Free strings after macro execution */
      KV_free(strKey);
      KV_free(strTemp);
      strKey = NULL;

      if (listInclude) {
        KV_AddInclude(&inclBaseFiles, listInclude);
        continue;
      }

      /* Or errored out */
      KV_DestroyIncludes(&inclIncludeFiles);
      KV_DestroyIncludes(&inclBaseFiles);
      KV_PairDestroy(list);
      return NULL;
    }

    /* Added a string value under some key */
    if (KV_AddStringPair(ctx, list, strKey, strTemp)) {
      KV_free(strKey);
      KV_free(strTemp);
      strKey = NULL;
      continue;
    }

    /* Or errored out */
    KV_free(strKey);
    KV_free(strTemp);

    KV_DestroyIncludes(&inclIncludeFiles);
    KV_DestroyIncludes(&inclBaseFiles);
    KV_PairDestroy(list);
    return NULL;
  }

  /* Append included pairs */
  for (iInclude = 0; iInclude < inclIncludeFiles.ctUsed; ++iInclude)
  {
    if (!KV_AppendIncludedPairs(ctx, list, inclIncludeFiles.aLists[iInclude]))
    {
      /* We were so close */
      KV_DestroyIncludes(&inclIncludeFiles);
      KV_DestroyIncludes(&inclBaseFiles);
      KV_PairDestroy(list);
      return NULL;
    }
  }

  /* Merge base pairs */
  for (iInclude = 0; iInclude < inclBaseFiles.ctUsed; ++iInclude)
  {
    if (!KV_MergeBasePairs(ctx, list, inclBaseFiles.aLists[iInclude]))
    {
      /* We were so close */
      KV_DestroyIncludes(&inclIncludeFiles);
      KV_DestroyIncludes(&inclBaseFiles);
      KV_PairDestroy(list);
      return NULL;
    }
  }

  /* Done parsing the buffer */
  KV_DestroyIncludes(&inclIncludeFiles);
  KV_DestroyIncludes(&inclBaseFiles);
  return list;
};

KV_Pair *KV_Parse(KV_Context *ctx) {
  assert(ctx);

  if (ctx->_file) {
    return KV_ParseFileInternal(ctx);
  }

  return KV_ParseBufferInternal(ctx, KV_false);
};

KV_Pair *KV_ParseBuffer(const char *buffer, size_t length) {
  KV_Context ctx;

  assert(buffer);
  KV_ContextSetupBuffer(&ctx, "", buffer, length);

  return KV_ParseBufferInternal(&ctx, KV_false);
};

KV_Pair *KV_ParseFile(const char *path) {
  KV_Context ctx;

  assert(path);
  KV_ContextSetupFile(&ctx, "", path);

  return KV_ParseFileInternal(&ctx);
};

KV_bool KV_Save(KV_Pair *pair, const char *path) {
  FILE *file;
  char *str;

  assert(pair && path);

  if (!pair || !path) {
    KV_SetError(NULL, "No pair or path specified");
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

  str = KV_Print(pair, NULL, 1048576, "\t"); /* 1MB step */

  if (!str) {
    fclose(file);
    return KV_false;
  }

  fputs(str, file);
  KV_free(str);

  fclose(file);
  return KV_true;
};
