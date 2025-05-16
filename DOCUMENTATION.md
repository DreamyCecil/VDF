# Table of Contents

- [Prelude](#Prelude)
- [Format specification](#Format-specification)
  - [Comments](#Comments)
  - [Key-value pairs](#Key-value-pairs)
  - [Key-value lists](#Key-value-lists)
  - [Macros](#Macros)
- [Memory management](#Memory-management)

# Prelude

This is a simple reader/writer of Valve Data Format (a.k.a. KeyValues) written in ANSI C.

This documentation briefly goes through the specifics of the format and how it can be manipulated using this library.

The main header file (`keyvalues.h`) includes extensive comments before each function explaining what they do and how to use them.

Sample code with usage examples can be found [here](samples).

# Format specification

The kind of input data that the parser expects is very simple. It has been implemented in respect to the description on the Valve Developer Community wiki: https://developer.valvesoftware.com/wiki/KeyValues

## Comments
The original format only supports CPP-styled single-line comments but this library also supports C-styled block comments.

**Valid comments:**
```js
// C++ single-line comment
/* C block comment */ "key1"  "value1"

/*
  Multi-line
  comment
*/
"key2"  "value2"

// Single slashes are ignored instead of being parsed as comments or as strings
/ "this is" / "not a comment" // "and this is"  "a comment"

"key3" // Next line contains a value for this key
"value3"
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
| `#base`    | Recursively merges key-value pairs of the specified file with the currently parsed list, preserving already existing values under the same keys. | `#base "C:\\absolute_path\\to_file\\on_disk.txt"` |
| `#include` | Appends key-value pairs of the specified file at the end of the currently parsed list. | `#include "OrMaybeRelativeToCWD.txt"` |

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
You can manage the library memory yourself instead of using the standard `malloc`, `free` and similar functions, if you so choose.

1. Compile the library with an extra `VDF_MANAGE_MEMORY` preprocessor definition (`-DVDF_MANAGE_MEMORY=1` CMake flag).
2. Set the following function pointers to your own functions:

| Function pointer | Default value | Purpose |
| ---------------- | ------------- | ------- |
| `KV_malloc`      | `malloc`      | Random memory allocation. |
| `KV_calloc`      | `calloc`      | Random memory allocation with nullified data. |
| `KV_realloc`     | `realloc`     | Random memory reallocation that preserves previous data. |
| `KV_free`        | `free`        | Random memory freeing. |
| `KV_strdup`      | `strdup`      | Duplication of a null-terminated string. |

> [!IMPORTANT]
> You have to redefine all of them together to ensure proper behavior.
