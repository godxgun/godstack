+++
title = 'Getting Started'
description = 'Compile a minimal C99 program that embeds the Grit single-header library.'
weight = 60
[params]
  article = true
  id = 'getting-started'
  product = 'Godstack'
  category = 'Getting started'
  tags = ['C', 'C99', 'Grit', 'compile']
  access = 'free'
  agent_setup = '''For a minimal CPU-only start, copy Grit.h into your project and define GRIT_IMPLEMENTATION in exactly one C translation unit before including it (`#define GRIT_IMPLEMENTATION` then `#include "Grit.h"`). Compile the complete getting-started program with `cc -std=c99 -Wall -Wextra -I. hello.c -o hello`; it prints `value: 42`. Other libraries have their own setup and dependencies; Poof is static header-only and does not use an implementation macro. To build a repository demo, bootstrap with `cc build.c -o build`, then run `./build demo cast`.'''
+++

# Build a first program

Godstack libraries are single-header C libraries, simply download the raw file from the repo, clone the entire repo then copy the file 
or add the repo as a submodule; whatever you prefer. 

Once that is done, include the file directly.

## Peak - Platform Layer

Peak was the first library to be written because it's the most fundamental layer you will want to bring to almost
any program. Peak allows you to run your programs on any of the suported platforms (as of writing: Windows, Linux, MacOS 
and web). Almost no setup is required since Peak auto detects the platform at compile time and links the correct
libraries at run time.

[Main Peak Article]()

## Poof - A Build System

You don't have to use poof if you prefer other tools, but it is quite convenient.

