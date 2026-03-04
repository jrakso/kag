# Kag Compiler

A small compiler written in **C** that translates a simple language into **x86-64 assembly (NASM)** and produces a Linux executable.

Kag process:

```
Source → Tokenizer → Parser (AST) → Codegen → output.asm
```

The generated assembly is assembled with **NASM** and linked with **ld**.

---

## Features

The language currently supports:

- integer literals
- string literals
- variables (`let`)
- assignment
- arithmetic (`+ - * /`)
- scopes `{ }`
- `if / elif / else`
- single line comments (`//`)
- `exit()` syscall

Example:

```
let x = 0;
let y = 1;
if (x) {      // false
    exit(x);
} elif (y) {  // true
    exit(y);
} else {
  let z = "string";
  exit(2);
}
```

---

## Build

Requirements:

- gcc
- nasm
- ld
- make

Build the compiler:

```
make
```

---

## Run

Compile and run:

```
make run FILE=program.kag
```

This will:

1. run the compiler
2. generate `output.asm`
3. assemble it with NASM
4. link it
5. run the program

---