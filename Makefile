# Build / test entry points.
#
#   make build          - build the broker (Linux only: epoll)            -> stompbroker.out
#   make test           - unit tests: existing + WebSocket (test.out) and STOMP under ASan/UBSan
#   make test-ws        - portable WebSocket unit tests under ASan/UBSan    -> test_websocket.out
#   make test-stomp     - portable STOMP unit tests under ASan/UBSan        -> test_stomp.out
#                         (both run on Linux and macOS)
#   make test-tsan      - STOMP unit tests (incl. the cross-thread close test) under ThreadSanitizer
#   make test-e2e       - end-to-end WebSocket + raw TCP STOMP tests, broker started on a free port (Linux only);
#                         includes examples/js/raw/stomp.protocol.test.js when node is installed
#   make test-e2e-asan  - raw TCP STOMP end-to-end test against an ASan/UBSan broker (Linux only)
#   make docker-test    - run all of the above (except test-tsan) inside a Linux container (podman or docker)
#   make docker-test-tsan - test-tsan inside the container (needs seccomp=unconfined)

CC       = gcc
CFLAGS   = -std=gnu11 -Wall -pthread
SANFLAGS = -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined
TSANFLAGS = -g -O1 -fno-omit-frame-pointer -fsanitize=thread
LDLIBS   = -lpthread

CONTAINER_ENGINE ?= $(shell command -v podman 2>/dev/null || command -v docker 2>/dev/null)
CONTAINER_IMAGE  ?= stompbroker-test

.PHONY: all build build-asan test test-ws test-stomp test-tsan test-e2e test-e2e-asan docker-test docker-test-tsan docker-image clean

all: build test

build:
	./build.sh

# Broker with sanitizers, for the end-to-end robustness run.
build-asan: stompbroker_asan.out

stompbroker_asan.out: FORCE
	$(CC) $(CFLAGS) $(SANFLAGS) main.c -o $@ $(LDLIBS)

test: test.out test_stomp.out
	./test.out
	./test_stomp.out

test.out: FORCE
	$(CC) $(CFLAGS) test.c -o $@ $(LDLIBS)

test-ws: test_websocket.out
	./test_websocket.out

test_websocket.out: FORCE
	$(CC) $(CFLAGS) $(SANFLAGS) test_websocket_main.c -o $@ $(LDLIBS)

test-stomp: test_stomp.out
	./test_stomp.out

test_stomp.out: FORCE
	$(CC) $(CFLAGS) $(SANFLAGS) test_stomp_main.c -o $@ $(LDLIBS)

test-tsan: test_stomp_tsan.out
	./test_stomp_tsan.out

test_stomp_tsan.out: FORCE
	$(CC) $(CFLAGS) $(TSANFLAGS) test_stomp_main.c -o $@ $(LDLIBS)

test-e2e: build
	python3 e2e/websocket_e2e.py --broker ./stompbroker.out --port $(E2E_PORT)
	python3 e2e/stomp_tcp_e2e.py --broker ./stompbroker.out --js examples/js/raw/stomp.protocol.test.js

# Leaks are checked by the unit tests; here ASan/UBSan catch memory errors under real traffic.
test-e2e-asan: build-asan
	ASAN_OPTIONS=detect_leaks=0 python3 e2e/stomp_tcp_e2e.py --broker ./stompbroker_asan.out --log e2e_stomp_asan.log

E2E_PORT ?= 3499

clean:
	rm -f *.out e2e_broker.log e2e_stomp_broker.log e2e_stomp_asan.log

docker-image:
	@test -n "$(CONTAINER_ENGINE)" || { echo "Neither podman nor docker found"; exit 1; }
	$(CONTAINER_ENGINE) build -f docker/test.Dockerfile -t $(CONTAINER_IMAGE) .

docker-test: docker-image
	$(CONTAINER_ENGINE) run --rm $(CONTAINER_IMAGE)

# ThreadSanitizer must turn off ASLR (personality()), which the default seccomp profile forbids.
docker-test-tsan: docker-image
	$(CONTAINER_ENGINE) run --rm --security-opt seccomp=unconfined $(CONTAINER_IMAGE) make test-tsan

FORCE:
