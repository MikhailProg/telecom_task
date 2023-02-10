#/bin/sh

set -eu

ADDRS=
PROGS=5

rand_num() {
    while :; do
        # gen 0 - 255
        addr=$(od -An -N1 -i /dev/random)
        echo "$@" | grep -qw "${addr}" || break
    done
    echo ${addr}
    unset addr
}

for i in $(seq ${PROGS}); do
    ADDR=$(rand_num $ADDRS)
    ADDRS="${ADDRS} ${ADDR}"
    HOST_ADDR=${ADDR} ./prog &
done

HOST_ADDR=$(rand_num ${ADDRS}) CONTROLLER= ./prog &

wait
echo 'ALL progs stopped'

