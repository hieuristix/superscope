# Virtual Superscope
A distributed system project simulating satellite collaboration for taking
super-resolution images.

Author:
Minh Hieu Nguyen
Akshay Krishnan Anilkumar Manjusha
Vaishnave Kunnuvakkam Vinjimoor
Enrique Job Calderon Olalde

## Overview

## Setup
First, clone this repo with all its submodule:
```bash
git clone --recursive https://github.com/hieuristic/superscope.git
```
If you already clone but not recursively, you can run:
```bash
git submodule update --init --recursive
```

We use justfile script to simply the compilation. To compile either `viewer` or
`superscope`, you can run
```
just com <target>
```
where `<target>` can be replaced with `viewer` or `superscope`.

Similarly, to run either the `viewer` or `superscope`, you can run:
```
just com <target>
```
