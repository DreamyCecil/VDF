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

int main(int argc, char *argv[]) {
  printf("---------------- ERRORS ----------------\n");

  KV_Context ctx;
  KV_Pair *list;


  // See if an unclosed string will be detected
  printf("-- Unclosed string\n");

  list = KV_ParseBuffer("\"asdf", -1);

  if (!list) {
    printf("SUCCESS - %s\n", KV_GetError());
  } else {
    printf("FAIL - No error occurred\n");
    KV_PairDestroy(list);
  }


  // See if removing a key from a pair and then printing it will be detected
  printf("\n-- Printing a non-list pair with no key\n");

  list = KV_ParseBuffer("try this", -1);

  if (!list) {
    printf("FAIL - %s\n", KV_GetError());

  } else {
    // Remove the key from "try" pair
    KV_Pair *pair = KV_FindPair(list, "try");
    KV_SetKey(pair, NULL);

    char *str = KV_Print(pair, NULL, 32, "");

    if (!str) {
      printf("SUCCESS - %s\n", KV_GetError());
    } else {
      printf("FAIL - No error occurred\n");
      KV_free(str);
    }

    KV_PairDestroy(list);
  }


  // See if pairs with the same key without multi-key support will be detected
  printf("\n-- Duplicate keys\n");

  // Test non-list pairs in the same file
  KV_ContextSetupFile(&ctx, "", "sample.vdf");
  KV_ContextSetFlags(&ctx, KV_false, KV_false, KV_false);

  list = KV_Parse(&ctx);

  if (!list) {
    printf("SUCCESS (string value) - %s\n", KV_GetError());
  } else {
    printf("FAIL (string value) - No error occurred\n");
    KV_PairDestroy(list);
  }

  // Test list pairs with included files
  KV_ContextSetupBuffer(&ctx, "", "#include include_1.vdf\n\nkey3 {}\n\n", -1);
  KV_ContextSetFlags(&ctx, KV_false, KV_false, KV_false);

  list = KV_Parse(&ctx);

  if (!list) {
    printf("SUCCESS (list value) - %s\n", KV_GetError());
  } else {
    printf("FAIL (list value) - No error occurred\n");
    KV_PairDestroy(list);
  }

  return 0;
};
