# PJS (Preprocessor-based JSON)

PJS is a wrapper around the Jansson JSON library that facilitates conversion from JSON to C structures and vice-versa.  In essence, it is a replacement for traditional IDL compilers implemented in pure C macros and as such doesn't require an extra step to generate intermediate  files.

## API

The first step is to define a message format. The message format is defined with a set of PJS nested macros. This is a short example:

```C
PJS(my_message,
        PJS_INT(my_integer),
        PJS_REAL(my_real),
        PJS_BOOL(my_bool),
        PJS_STRING(my_string, 256))
```

This roughly translates to the following JSON/C structure counterparts:

* C-structure:

```C
    struct my_message
    {
        int my_int
        double my_real;
        bool my_bool;
        char my_string[256];
    }
```

* JSON:

```JSON
    {
        "my_int": value,
        "my_real": real_value,
        "bool": true/false,
        "my_string": "string value",
    }
```

## Types

### Basic Types

| PJS type              |  C type            | JSON type                |
| --------------------- | ------------------ | ------------------------ |
| PJS_INT(N)            | int N;             | "N": integer             |
| PJS_BOOL(N)           | bool N;            | "N": true or false       |
| PJS_REAL(N)           | double N;          | "N": float number        |
| PJS_STRING(N, LEN)    | char N[LEN];       | "N": "string value"      |
| PJS_SUB(N, SUB)       | struct SUB N;      | "N": { ... }             |


### Optional Basic Types

Optional basic types are similar to basic types except that they define an additional field in the C structure and do not produce errors if the fields are missing during JSON parsing. The additional field is a boolean type and is  set to true if the field exists in the JSON when parsing.

| PJS type              |  C type                       | JSON type                |
| --------------------- | ----------------------------- | ------------------------ |
| PJS_INT_Q(N)          | int N; bool N_exists;         | "N": integer             |
| PJS_BOOL_Q(N)         | bool N; bool N_exists;        | "N": true or false       |
| PJS_REAL_Q(N)         | double N; bool N_exists;      | "N": float number        |
| PJS_STRING_Q(N, LEN)  | char N[LEN]; bool N_exists;   | "N": "string value"      |
| PJS_SUB_Q(N, SUB)     | struct SUB N; bool N_exists;  | "N": { ... }             |


### Array Types

| PJS type                  |  C type                       | JSON type                     |
| ------------------------- | ----------------------------- | ----------------------------- |
| PJS_INT_A(N, SZ)          | int N[SZ]; int N_len;         | "N": [ integer, ... ]         |
| PJS_BOOL_A(N, SZ)         | bool N[SZ]; int N_len;        | "N": [ true or false, ...]    |
| PJS_REAL_A(N, SZ)         | double N[SZ]; int N_len;      | "N": [ float, ... ]           |
| PJS_STRING_A(N, LEN, SZ)  | char N[LEN][SZ]; int N_len;   | "N": [ "string value", ... ]  |
| PJS_SUB_A(N, SUB, SZ)     | struct SUB N[SZ]; int N_len;  | "N": [ { ... }, ... ]         |

### Optional Array Types

| PJS type                  |  C type                       | JSON type                     |
| ------------------------- | ----------------------------- | ----------------------------- |
| PJS_INT_QA(N, SZ)         | int N[SZ]; int N_len;         | "N": [ integer, ... ]         |
| PJS_BOOL_QA(N, SZ)        | bool N[SZ]; int N_len;        | "N": [ true or false, ...]    |
| PJS_REAL_QA(N, SZ)        | double N[SZ]; int N_len;      | "N": [ float, ... ]           |
| PJS_STRING_QA(N, LEN, SZ) | char N[LEN][SZ]; int N_len;   | "N": [ "string value", ... ]  |
| PJS_SUB_QA(N, SUB, SZ)    | struct SUB N[SZ]; int N_len;  | "N": [ { ... }, ... ]         |

