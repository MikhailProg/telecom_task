FROM alpine:latest as builder
RUN apk add --no-cache gcc make libc-dev
COPY src /src/
WORKDIR /src
RUN make clean && make

FROM alpine:latest as runner
WORKDIR /srv/telco
COPY --from=builder /src/*.sh /src/prog .
CMD ./run.sh
