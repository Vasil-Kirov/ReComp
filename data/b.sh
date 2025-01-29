#!/bin/bash

rcp build.rcp $1 $2 $3 --link -lm || exit 1

