# Linux build + test image. Works with both `podman build` and `docker build`:
#   podman build -f docker/test.Dockerfile -t stompbroker-test . && podman run --rm stompbroker-test
# or simply: make docker-test
FROM docker.io/library/gcc:13

WORKDIR /src
COPY . /src

RUN make clean build
CMD ["make", "test", "test-ws", "test-e2e", "test-e2e-asan"]
