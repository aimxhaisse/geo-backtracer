# Covid Backtracer.
#
# For now, all-in-one binary; this is meant to be split at some point.

DEPS 		:= deps

DEPS_GTEST_DIR  := $(DEPS)/gtest
DEPS_GTEST 	:= $(DEPS)/gtest/build/lib/libgtest.a

DEPS_CXXFLAGS	:= -I$(DEPS)/gtest/googletest/include
DEPS_LDFLAGS	:= -L$(DEPS)/gtest/build/lib

ALL_DEPS 	:= $(DEPS_GTEST)

CXXFLAGS = -O3 -Wall -Wno-unused-local-typedef -Wno-deprecated-declarations -std=c++17 $(shell freetype-config --cflags 2>/dev/null) -I. -I/usr/local/include/google/protobuf $(DEPS_CXXFLAGS)
LDLIBS = -lglog -lgflags -lrocksdb -lboost_filesystem -lgrpc++ -lprotobuf -lyaml-cpp -lpthread -lboost_system -lz -ldl -lzstd -lsnappy -lbz2 -llz4 $(DEPS_LDFLAGS)

SERVER := build/bt
CLIENT := build/client
DAEMON := build/daemonizer
TEST   := build/bt_test
CXX    := clang++
FMT    := clang-format
PBUF   := protoc

SRCS_SERVER := $(filter-out $(wildcard server/*test.cc), $(wildcard server/*.cc))
OBJS_SERVER := $(SRCS_SERVER:.cc=.o)
DEPS_SERVER := $(OBJS_SERVER:.o=.d)

SRCS_CLIENT := $(wildcard client/*.cc)
OBJS_CLIENT := $(SRCS_CLIENT:.cc=.o)
DEPS_CLIENT := $(OBJS_CLIENT:.o=.d)

SRCS_COMMON := $(filter-out $(wildcard common/*test.cc), $(wildcard common/*.cc))
OBJS_COMMON := $(SRCS_COMMON:.cc=.o)
DEPS_COMMON := $(OBJS_COMMON:.o=.d)

SRCS_TEST := $(filter-out server/main.cc, $(wildcard server/*.cc)) $(wildcard common/*.cc)
OBJS_TEST := $(SRCS_TEST:.cc=.o)
DEPS_TEST := $(OBJS_TEST:.o=.d)

SRCS_PB := $(wildcard proto/*.proto)
GENS_PB := $(SRCS_PB:.proto=.pb.cc)
OBJS_PB := $(SRCS_PB:.proto=.pb.o)
HEAD_PB := $(SRCS_PB:.proto=.pb.h) $(SRCS_PB:.proto=.pb.d)

SRCS_GRPC := $(wildcard proto/*.proto)
GENS_GRPC := $(SRCS_GRPC:.proto=.grpc.pb.cc)
OBJS_GRPC := $(SRCS_GRPC:.proto=.grpc.pb.o)
HEAD_GRPC := $(SRCS_GRPC:.proto=.grpc.pb.h) $(SRCS_GRPC:.proto=.grpc.pb.d)

GTEST_VERSION 	:= v1.10.x

INSTALL_DIR	?= ./
PYTHONPATH      ?= /usr/local/Cellar/ansible/2.9.7/libexec/lib/python3.8/site-packages

.PHONY: all clean re test fmt help run inject server client

help:
	@echo "Help for Covid Backtracer:"
	@echo ""
	@echo "\033[1;31mThis is not production ready, some commands here are destructive.\033[0m"
	@echo ""
	@echo "make		# this message"
	@echo "make all	# build everything"
	@echo "make test	# run unit tests"
	@echo "make clean	# clean all build artifacts"
	@echo "make re		# rebuild covid backtracer"
	@echo "make server	# run a local instance of covid backtracer"
	@echo "make install     # install built binaries to INSTALL_DIR"
	@echo "make pytest      # run python unit tests"
	@echo ""

all: $(SERVER) $(CLIENT) $(TEST) $(DAEMON)

fmt:
	$(FMT) -i -style Chromium $(SRCS_SERVER) $(SRCS_CLIENT) $(SRCS_COMMON)

clean:
	rm -rf $(OBJS_SERVER) $(DEPS_SERVER) $(SERVER)
	rm -rf $(OBJS_CLIENT) $(DEPS_CLIENT) $(CLIENT)
	rm -rf $(OBJS_COMMON) $(DEPS_COMMON)
	rm -rf $(OBJS_TEST) $(DEPS_TEST) $(TEST)
	rm -rf $(GENS_PB) $(OBJS_PB) $(HEAD_PB)
	rm -rf $(GENS_GRPC) $(OBJS_GRPC) $(HEAD_GRPC)

bin:
	mkdir -p bin

build:
	mkdir -p build

re: clean all

%.o: %.cc $(ALL_DEPS)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

%.grpc.pb.cc: %.proto
	$(PBUF) --grpc_out=. --plugin=protoc-gen-grpc=$(shell which grpc_cpp_plugin) $<

%.pb.cc: %.proto
	$(PBUF) --cpp_out=. $<

%.pb.o : %.pb.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(SERVER): build $(GENS_PB) $(GENS_GRPC) $(OBJS_SERVER) $(OBJS_PB) $(OBJS_GRPC) $(OBJS_COMMON)
	$(CXX) $(OBJS_SERVER) $(OBJS_PB) $(OBJS_GRPC) $(OBJS_COMMON) $(LDLIBS) -o $@

$(CLIENT): build $(OBJS_COMMON) $(GENS_PB) $(OBJS_CLIENT) $(OBJS_PB)
	$(CXX) $(OBJS_CLIENT) $(OBJS_PB) $(OBJS_GRPC) $(OBJS_COMMON) $(LDLIBS) -o $@

$(TEST): build $(GENS_PB) $(GENS_GRPC) $(OBJS_GRPC) $(OBJS_TEST) $(OBJS_PB)
	$(CXX) $(OBJS_TEST) $(OBJS_PB) $(OBJS_GRPC) $(LDLIBS) -lgtest -o $@

$(DAEMON): build
	clang -O3 daemonizer/daemonizer.c -o $@

$(DEPS):
	mkdir -p $@

$(DEPS_GTEST_DIR): $(DEPS)
	git clone https://github.com/google/googletest.git $@
	touch $(DEPS)/*
	cd $(DEPS_GTEST_DIR) && git checkout origin/$(GTEST_VERSION) -b $(GTEST_VERSION)

$(DEPS_GTEST): $(DEPS_GTEST_DIR)
	cd $(DEPS_GTEST_DIR) && mkdir -p build && cd build && cmake .. && make
	touch $@

install: $(BIN) $(DAEMON) $(SERVER)
	rsync all.bash $(INSTALL_DIR) # rsync to handle copy to self (local).
	-pkill -9 client
	-(cd $(INSTALL_DIR) ; ./all.bash mixer stop ; ./all.bash worker stop)
	sleep 10 # leave some time for servers to quit.
	cp $(SERVER) $(INSTALL_DIR)/bin/bt
	cp $(DAEMON) $(INSTALL_DIR)/bin/daemonizer
	cp $(CLIENT) $(INSTALL_DIR)/bin/client
	(cd $(INSTALL_DIR) ; ./all.bash mixer start ; ./all.bash worker start)

server: $(SERVER)
	$(SERVER)

test: $(TEST)
	$(TEST)

# Quick & dirty way to run unit tests, this should evolve if there is
# a need to improve the geo-bt ansible module, which is not expected
# for now. This only works on mac os, at a specific point in time, with
# ansible installed from brew.
pytest:
	PYTHONPATH=$(PYTHONPATH) python3 prod/playbooks/library/bt_shard_test.py

-include $(DEPS_SERVER)
-include $(DEPS_CLIENT)
-include $(DEPS_COMMON)
-include $(DEPS_TEST)
