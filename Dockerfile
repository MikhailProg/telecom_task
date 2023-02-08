FROM alpine:latest as builder
RUN apk add --no-cache gcc make libc-dev
COPY src /src/
RUN make -C /src clean && make -C /src

FROM alpine:latest as runner
WORKDIR /srv/telco
COPY --from=builder /src/*.sh /src/prog .
CMD ./run.sh
