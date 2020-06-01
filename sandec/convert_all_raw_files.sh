#!/bin/bash

ls *.raw | xargs -i wine ./sandec.exe.so {}
