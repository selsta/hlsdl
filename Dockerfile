FROM alpine:latest as builder

WORKDIR /hlsdl-repo

RUN --mount=type=cache,target=/var/cache/apk \
    apk add gcc make curl-dev libc-dev

COPY makefile hlsdl.1 LICENSE ./

COPY src src

RUN make && make install && make clean


FROM alpine:latest

RUN --mount=type=cache,target=/var/cache/apk \
    apk add curl

RUN mkdir -p /var/hlsdl/data && chown 1000:1000 /var/hlsdl/data

USER 1000:1000

VOLUME /var/hlsdl/data

WORKDIR /var/hlsdl/data

COPY --from=builder /usr/local/bin/hlsdl /usr/local/bin/

CMD ["hlsdl"]
