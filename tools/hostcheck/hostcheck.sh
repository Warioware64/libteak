#!/bin/bash
#
# SPDX-License-Identifier: CC0-1.0
#
# Checks a dsp-bench example without hardware: the DSP code (teak/source/main.c
# and common/*.c) is built for the host, every job is run on the host and in
# teaksim (the DSP binary in the teakra emulator), and their checksums are
# compared.
#
# usage: hostcheck.sh <example dir> <number of jobs> [host stubs .c] [setup file]
#
# - The host stubs file replaces the assembly functions of the example by their
#   C versions (and proto_dma_out() by a stub).
# - The setup file uploads tables and sets parameters before the jobs:
#       U <table> <words> <word> <word> ...   (hexadecimal)
#       P <id> <value>
#
# Needs ../teaksim/teaksim (make -C ../teaksim) and the example built with make.

H=$(cd "$(dirname "$0")" && pwd)
TEAKSIM=$H/../teaksim/teaksim
OUT=$(mktemp -d)
E=$1; N=$2; STUB=${3:-}; SETUP=${4:-}
gcc -O2 -w -DHOST -I$H/stub -I$E/common -I$E/teak/source $H/hostmain.c $E/teak/source/main.c $E/common/*.c $STUB -o $OUT/host_bin || exit 1
$OUT/host_bin $N $SETUP > $OUT/host.txt
make -C $E 2>&1 | grep -iE " error|fatal|warning" | head -5
{
  seq=1
  next() { seq=$(( seq % 15 + 1 )); }
  if [ -n "$SETUP" ]; then
    while read -r kind a b rest; do
      if [ "$kind" = U ]; then
        next; printf 'cmd 1 %X %X\n' $(( 0x1000 | (seq<<8) | 0x$a )) $(( 0x$b ))
        words=($rest); for ((i=0;i<${#words[@]};i+=2)); do next; hi=${words[i]}; lo=${words[i+1]:-0}; printf 'cmd 1 %X %X\n' $(( 0x2000 | (seq<<8) )) $(( (0x$hi<<16) | 0x$lo )); done
      elif [ "$kind" = P ]; then
        next; printf 'cmd 1 %X %X\n' $(( 0x5000 | (seq<<8) | 0x$a )) $(( 0x$b ))
      fi
    done < "$SETUP"
  fi
  for ((j=0;j<N;j++)); do next; printf 'cmd 2 %X 1\n' $(( 0x3000 | (seq<<8) | j )); done
  echo quit
} | timeout 600 $TEAKSIM $E/build/teak.elf 2>/dev/null | grep -E "^rep [0-9A-F]" | awk 'BEGIN{j=0} { r=substr($2,1,1); if (r=="1") { cyc[j-1]=$3 } else { st[j]=$2; cs[j]=$3; j++ } } END { for (i=0;i<j;i++) print st[i], cs[i], cyc[i] }' | tail -n $N > $OUT/dsp.txt
paste -d' ' $OUT/host.txt $OUT/dsp.txt | awk '{ ok = ($3==$5) ? "ok" : "<-- DIFF"; printf "job %2d host %s dsp %s status %s cycles %d %s\n", $1, $3, $5, substr($4,4,1), strtonum("0x"$6), ok }'
rm -rf $OUT
