#/bin/sh

set -e

NUMS=""

for i in $(seq 5); do
    while :; do
        # gen 0 - 255
        NUM=$(od -An -N1 -i /dev/random)
        echo "${NUMS}" | grep -qw "${NUM}" || break 
    done

    NUMS="${NUMS} ${NUM}"
    HOST_NUM=$((NUM+1)) ./prog &
done

wait
echo 'ALL progs stopped'

