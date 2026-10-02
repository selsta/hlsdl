FROM alpine:latest AS builder

WORKDIR /hlsdl-repo

RUN --mount=type=cache,target=/var/cache/apk \
    apk add cmake ninja gcc libc-dev curl-dev openssl-dev

COPY CMakeLists.txt hlsdl.1 LICENSE ./

COPY src src

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && cmake --install build


FROM alpine:latest

RUN --mount=type=cache,target=/var/cache/apk \
    apk add curl

RUN mkdir -p /var/hlsdl/data && chown 1000:1000 /var/hlsdl/data

USER 1000:1000

VOLUME /var/hlsdl/data

WORKDIR /var/hlsdl/data

COPY --from=builder /usr/local/bin/hlsdl /usr/local/bin/

CMD ["hlsdl"]
