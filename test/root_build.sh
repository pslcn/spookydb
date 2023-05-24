#!/bin/sh

set -xe

doas chown root:root $1
doas chmod u+s $1
