#!/bin/bash
#David Shagam
#Operating Systems 460
#Run me for getting all outputs

make
./PageAlgs -lru > lru.txt
./PageAlgs -lfu> lfu.txt