NODE_PATH := $(shell pwd)/lib

all: build

configure:
        node-gyp configure

build:
        node-gyp build

clean:
        node-gyp clean
        rm -rf build

.PHONY: configure clean build