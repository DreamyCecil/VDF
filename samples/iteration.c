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
#include <stdio.h>
#include <string.h>
#include "../keyvalues.h"

int main(int argc, char *argv[])
{
  KV_List *list = KV_ParseFile("sample.vdf");

  if (!list) {
    fprintf(stderr, "%s\n", KV_GetError());
    return 1;
  }

  KV_Pair *pair;


  // Create an array that will contain references to pairs with list values
  size_t ct = KV_ListCount(list);
  KV_Pair **aLists = (KV_Pair **)KV_calloc(ct, sizeof(KV_Pair *));


  // List each pair until there is no more
  size_t i = 0;
  printf("-- Pairs in the list:\n");

  while ((pair = KV_GetPair(list, i++)))
  {
    // Remember lists in the process
    int bList = (KV_GetDataType(pair) == KV_TYPE_NONE);
    if (bList) aLists[i-1] = pair;

    printf("\"%s\" is a %s\n", KV_GetKey(pair), bList ? "list" : "string");
  }


  // Replace value in each pair with the same string
  KV_Pair *iter = KV_GetHead(list);

  for (; iter; iter = KV_GetNext(iter))
  {
    KV_PairSetString(iter, KV_GetKey(iter), "asdf");
  }


  // Print pairs that used to contain lists
  char *buffer;
  printf("\n-- List values after replacement:\n");

  for (i = 0; i < ct; ++i) {
    if (!aLists[i]) continue;

    buffer = KV_PairPrint(aLists[i], NULL, 1024, " = ");
    printf("%s", buffer);
    KV_free(buffer);
  }


  KV_free(aLists);
  KV_ListDestroy(list);

  return 0;
};
