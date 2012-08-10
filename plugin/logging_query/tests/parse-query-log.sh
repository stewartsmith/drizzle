#!/bin/sh
wc -l "$1"|awk '{print($1)}'
