# Build / test entry points.
#
#   make build        - build the broker (Linux only: epoll)            -> stompbroker.out
#   make test         - existing unit tests + WebSocket unit tests        -> test.out
#   make test-ws      - portable WebSocket unit tests under ASan/UBSan    -> test_websocket.out
#                       (runs on Linux and macOS)
#   make test-e2e     - end-to-end WebSocket test against a running broker (Linux only)
#   make docker-test  - run all of the above inside a Linux container (podman or docker)

CC       = gcc
CFLAGS   = -std=gnu11 -Wall -pthread
SANFLAGS = -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined
LDLIBS   = -lpthread

CONTAINER_ENGINE ?= $(shell command -v podman 2>/dev/null || command -v docker 2>/dev/null)
CONTAINER_IMAGE  ?= stompbroker-test

.PHONY: all build test test-ws test-e2e docker-test docker-image clean

all: build test

build:
	./build.sh

test: test.out
	./test.out

test.out: FORCE
	$(CC) $(CFLAGS) test.c -o $@ $(LDLIBS)

clean:
	rm -f *.out

docker-image:
	@test -n "$(CONTAINER_ENGINE)" || { echo "Neither podman nor docker found"; exit 1; }
	$(CONTAINER_ENGINE) build -f docker/test.Dockerfile -t $(CONTAINER_IMAGE) .

docker-test: docker-image
	$(CONTAINER_ENGINE) run --rm $(CONTAINER_IMAGE)

FORCE:
