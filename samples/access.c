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
#include "../keyvalues.h"

int main(int argc, char *argv[])
{
  KV_List *listFile = KV_ParseFile("sample.vdf");

  if (!listFile) {
    fprintf(stderr, "%s\n", KV_GetError());
    return 1;
  }

  KV_Pair *pair;
  KV_List *list;


  // Add the list to itself (copies all pairs up until this point)
  pair = KV_NewPairList("this", KV_ListCopy(listFile));

  KV_ListAppend(listFile, pair);


  // Add a pair to a list inside the last list (must exist)
  list = KV_GetList(pair);
  list = KV_FindList(list, "dummy");

  KV_ListAppend(list, KV_NewPairString("hello", "hi!"));


  // Check if there's a pair under the "Test" key
  pair = KV_FindPair(listFile, "Test");

  printf("'Test' pair %s\n", pair ? "exists" : "doesn't exist");


  // Check if some list is empty (must exist)
  list = KV_FindList(listFile, "dummy");

  printf("'dummy' list is %s\n", KV_IsListEmpty(list) ? "empty" : "not empty");


  // Retrieve a value from "this/dummy/hello"
  list = listFile;
  list = KV_FindList(list, "this");
  list = KV_FindList(list, "dummy");
  pair = KV_FindPair(list, "hello");

  printf("'this/dummy/hello' = %s\n", (KV_GetDataType(pair) == KV_TYPE_NONE) ? "{list}" : KV_GetString(pair));


  KV_ListDestroy(listFile);
  return 0;
};
