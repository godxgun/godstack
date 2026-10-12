# Getting Started

Documentation: https://example.org/guides/getting-started/
Description: Compile a minimal C99 program that embeds the Grit single-header library.


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

