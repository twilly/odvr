Introduction
============

Converts Olympus Digital Voice Recorder files to WAV files.


Building
========

There are two implementations:
- sandec.c which requires Wine and a DLL (san_dec.dll) to be found in the official Olympus Windows installer (this was the historically first implementation used with odvr and it remains here as an archive)
- nasced.c which is a pure Linux implementation (Makefile is configured to use that one by default)

To build:
- If you already built odvr, probably sandec is already built using nasced.c
- You can also build it running `make` in sandec folder.

Usage
=====

`sandec [filename].raw`

Copyright
=========
Copyright (C) 2008 Robert Mazur (rm@nateo.pl)
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
