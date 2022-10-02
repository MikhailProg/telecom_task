#/bin/sh

set -eu

ADDRS=

for i in $(seq 5); do
    while :; do
        # gen 0 - 255
        ADDR=$(od -An -N1 -i /dev/random)
        echo "${ADDRS}" | grep -qw "${ADDR}" || break
    done

    ADDRS="${ADDRS} ${ADDR}"
    HOST_ADDR=${ADDR} ./prog &
done

wait
echo 'ALL progs stopped'

