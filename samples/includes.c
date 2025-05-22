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

// After this is parsed and evaluated, the included pairs should be
// appended at the ends of the respective lists that the macros are in.
static const char *_include = " \
  #include include_1.vdf        \
  key1  custom_value            \
                                \
  key3 {                        \
    inner1  new_value           \
    inner3  false               \
                                \
    #include include_2.vdf      \
  }                             \
";

/* After this is parsed and evaluated, the resulting list should look like this:
  "key1"  "don't replace me!"
  "key3"
  {
    "inner1"  "new_value"
    "inner3"  "false"
    "inner4"  "base2_inner_2"  // This goes before "inner2" because "#base include_2.vdf" is executed before "#base include_1.vdf"
    "inner2"  "base1_inner_2"
  }
  "key2"  "base1_value_2"
*/
static const char *_base = " \
  #base include_1.vdf        \
  key1  custom_value         \
                             \
  key3 {                     \
    inner1  new_value        \
    inner3  false            \
                             \
    #base include_2.vdf      \
  }                          \
";

int main(int argc, char *argv[]) {
  printf("---------------- INCLUDES ----------------\n");

  KV_Pair *list;
  char *buffer;


  // Parse #include macros
  list = KV_ParseBuffer(_include, -1);

  if (!list) {
    fprintf(stderr, "%s\n", KV_GetError());
    return 1;
  }

  // Print out the list
  buffer = KV_Print(list, NULL, 1024, "\t");
  printf("-- #include macros:\n%s", buffer);
  KV_free(buffer);

  KV_PairDestroy(list);


  // Parse #base macros
  list = KV_ParseBuffer(_base, -1);

  if (!list) {
    fprintf(stderr, "%s\n", KV_GetError());
    return 1;
  }

  // Print out the list
  buffer = KV_Print(list, NULL, 1024, "\t");
  printf("\n-- #base macros:\n%s", buffer);
  KV_free(buffer);

  KV_PairDestroy(list);

  return 0;
};
