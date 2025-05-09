# Table of Contents

- [Prelude](#Prelude)
- [Format specification](#Format-specification)
  - [Comments](#Comments)
  - [Key-value pairs](#Key-value-pairs)
  - [Key-value lists](#Key-value-lists)
  - [Macros](#Macros)
- [Memory management](#Memory-management)
- [Usage examples](#Usage-examples)
  - [Quick reading](#Quick-reading)
  - [Creation and writing](#Creation-and-writing)
  - [Value access](#Value-access)
  - [Parser contexts](#Parser-contexts)

# Prelude

This is a simple reader/writer of Valve Data Format (a.k.a. KeyValues) written in ANSI C.

This documentation briefly goes through the specifics of the format and how it can be manipulated using this library.

The main header file (`keyvalues.h`) includes extensive comments before each function explaining what they do and how to use them.

# Format specification

The kind of input data that the parser expects is very simple. It has been implemented in respect to the description on the Valve Developer Community wiki: https://developer.valvesoftware.com/wiki/KeyValues

## Comments
This format supports single-line CPP-styled comments on any line. Based on the wiki article, they can start with just a single `/` character, however it's still advised to use double slashes (`//`) for consistency.

**Valid comments:**
```js
/ Comment with one slash (not recommended)
// Comment with double slash (recommended)
/* Block comments aren't supported */ and thus these tokens afterwards are ignored

"key" // Next line contains a value for this key
"value"
```

## Key-value pairs
Different value types under key names. Currently, there are only two value types: lists and strings.  
Strings can either be unquoted identifiers without whitespaces or any text enclosed in double quotes (`"`), including optional C-style escape sequences for special characters.

**Valid pairs:**
```js
key1      value_without_breaks
"key2"    "value with breaks"
"key 3"   123.0

// Tokens that break unquoted values: "{} \"\n"
nothing"between"
"here"too

escape_sequences        "\v\t\"\\"
"inferior line break"   \r\n

// Lists
list1{}
list2 { inner { "Hello" "World!" } }
```

## Key-value lists
Lists are collections of key-value pairs that go one after another.  
The initial input data is always parsed as a list (but without enclosing `{}` characters), otherwise it's parsed as a value for a preceeding key in a pair.

**Valid lists:**
```js
// Pairs in the initial global list

// Empty list value
key1 {}

// List value with more pairs
"key 2" {
  "pair inside"   "a list"
  "and another"   "one"
}

// Self-explanatory
list
{
  "inside a list"
  {
    "inside a list" {}
  }
}
```

## Macros
Macros are specific commands that are executed after parsing a proper key-value pair.

| Macro      | Purpose | Usage example |
| ---------- | ------- | ------------- |
| `#base`    | Includes key-value pairs of the specified file within currently parsed list. | `#base "C:\\absolute_path\\to_file\\on_disk.txt"` |
| `#include` | The exact same behavior as `#base`. | `#include "OrMaybeRelativeToCWD.txt"` |

**Valid macros:**
```js
// Adds all pairs from the included file to the global list before "inner"
#base  "AnotherList.txt"

"inner" {
  "key1"  Hello

  // Adds all pairs from the included file to the "inner" list between "key1" and "key2"
  "#include"  inner.txt

  "key2"  World
}

// Adds all pairs from the included file at the end of the global list
#include  Extras.txt
```

# Memory management
If you wish to manage the parser's memory yourself instead of using the standard `malloc`, `calloc`, `realloc` & `free` functions, you can redefine the following macros with your own functions before including `keyvalues.h`.

| Macro        | Default value | Purpose |
| ------------ | ------------- | ------- |
| `KV_malloc`  | `malloc`      | Random memory allocation. |
| `KV_calloc`  | `calloc`      | Random memory allocation with nullified data. |
| `KV_realloc` | `realloc`     | Random memory reallocation that preserves previous data. |
| `KV_free`    | `free`        | Random memory freeing. |

> [!IMPORTANT]
> You have to redefine all of them together to ensure proper behavior. If at least one of the macros isn't redefined while the others are, the compiler will display an error.

# Usage examples

Input file used by examples:
```js
key1  "value"
dummy {}
key2  "123.456"

key1  "duplicate\n\tkey"

"secret list" {
    "Half-Life 3"    "confirmed"
}
```

## Quick reading
```c
#include <stdio.h>
#include "keyvalues.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Please specify a path to a file\n");
    return 1;
  }

  // Parse a file
  KV_List *list = KV_ParseFile(argv[1]);

  // Output error, if any
  if (!list) {
    fprintf(stderr, "%s\n", KV_GetError());
    return 1;
  }

  // Print out the list
  char *buffer = KV_ListPrint(list, NULL, 1024, "\t");
  printf("%s\n", buffer);
  KV_free(buffer);

  // Destroy created list
  KV_ListDestroy(list);
  return 0;
};
```

## Creation and writing
```c
#include <stdio.h>
#include "keyvalues.h"

int main(int argc, char *argv[]) {
  // Create a list
  KV_List *list = KV_NewList();
  KV_Pair *pair;

  // Add one pair
  pair = KV_NewPairStringDup("Key1", "Hello, World!");
  KV_ListAppend(list, pair);

  // Add an empty list
  pair = KV_NewPairListDup("List", KV_NewList());
  KV_ListAppend(list, pair);

  // Add a pair to that empty list
  KV_ListAppend(KV_GetList(pair), KV_NewPairStringDup("Key2", "123.456"));

  // Save list into a file
  KV_Save(list, "out.txt");

  // Destroy created list
  KV_ListDestroy(list);
  return 0;
};
```

## Value access
```c
#include <stdio.h>
#include "keyvalues.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Please specify a path to a file\n");
    return 1;
  }

  KV_List *list = KV_ParseFile(argv[1]);

  if (!list) {
    fprintf(stderr, "%s\n", KV_GetError());
    return 1;
  }

  // Add itself (copies all pairs up until this point)
  KV_Pair *pair = KV_NewPairListDup("this", KV_ListCopy(list));
  KV_ListAppend(list, pair);

  // Add a pair to a list inside the last list (must exist)
  KV_ListAppend(KV_FindList(KV_GetList(pair), "dummy"), KV_NewPairStringDup("hello", "hi!"));

  // List current pairs
  size_t i = 0;
  printf("Pairs in the list:\n");

  while (pair = KV_GetPair(list, i++)) {
    printf("'%s' - %s value\n", KV_GetKey(pair), KV_HasListValue(pair) ? "list" : "string");
  }

  // Check if there's a pair under the "Test" key
  pair = KV_FindPair(list, "Test");
  printf("\n'Test' pair %s\n", pair ? "exists" : "doesn't exist");

  // Check if some list is empty (must exist)
  printf("'dummy' is %s\n", KV_IsListEmpty(KV_FindList(list, "dummy")) ? "empty" : "not empty");

  // Retrieve a value from "this/dummy/hello"
  pair = KV_FindPair(KV_FindList(KV_FindList(list, "this"), "dummy"), "hello");
  printf("'this/dummy/hello' = %s\n", KV_HasListValue(pair) ? "{list}" : KV_GetString(pair));

  KV_ListDestroy(list);
  return 0;
};
```

## Parser contexts
```c
#include <stdio.h>
#include "keyvalues.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Please specify a path to a file\n");
    return 1;
  }

  // Setup context for opening a file in the current working directory
  KV_Context ctx;
  KV_ContextSetupFile(&ctx, "", argv[1]);

  // Or for reading from a character buffer
  const char *example =
    "key1  \"duplicate\\n\\tkey\"  \n"
    "key2  {}";
  KV_ContextSetupBuffer(&ctx, "", example, -1);

  // Allow escape sequences and replace values under already existing keys
  KV_ContextSetFlags(&ctx, KV_true, KV_false, KV_true);

  // Parse within the context
  KV_List *list = KV_Parse(&ctx);

  // Output error, if any
  if (!list) {
    fprintf(stderr, "%s\n", KV_GetError());
    return 1;
  }

  // Print a value with escape sequences in it
  printf("key1 = '%s'\n", KV_FindString(list, "key1"));

  // Destroy created list
  KV_ListDestroy(list);
  return 0;
};
```
