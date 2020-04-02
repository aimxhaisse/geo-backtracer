# Covid Backtracer.
#
# For now, all-in-one binary; this is meant to be split at some point.

CXXFLAGS = -O3 -Wno-deprecated-declarations -Wall -std=c++17 $(shell freetype-config --cflags) -I.
LDLIBS = -lglog -lgflags -lrocksdb -lboost_filesystem -lgrpc++ -lprotobuf

SERVER := bin/bt_server
CLIENT := bin/bt_client
TEST   := bin/bt_test
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
	@echo "make client	# inject fixtures into local instance"
	@echo ""

all: $(SERVER) $(CLIENT) $(TEST)

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

re: clean all

%.o: %.cc
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

%.grpc.pb.cc: %.proto
	$(PBUF) --grpc_out=. --plugin=protoc-gen-grpc=$(shell which grpc_cpp_plugin) $<

%.pb.cc: %.proto
	$(PBUF) --cpp_out=. $<

%.pb.o : %.pb.cc
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

$(SERVER): bin $(GENS_PB) $(GENS_GRPC) $(OBJS_SERVER) $(OBJS_PB) $(OBJS_GRPC) $(OBJS_COMMON)
	$(CXX) $(OBJS_SERVER) $(OBJS_PB) $(OBJS_GRPC) $(OBJS_COMMON) $(LDLIBS) -o $@

$(CLIENT): bin $(OBJS_COMMON) $(GENS_PB) $(OBJS_CLIENT) $(OBJS_PB)
	$(CXX) $(OBJS_CLIENT) $(OBJS_PB) $(OBJS_GRPC) $(OBJS_COMMON) $(LDLIBS) -o $@

$(TEST): bin $(OBJS_GRPC) $(OBJS_TEST) $(OBJS_PB)
	$(CXX) $(OBJS_TEST) $(OBJS_PB) $(OBJS_GRPC) $(LDLIBS) -lgtest -o $@

server: $(SERVER)
	$(SERVER)

client: $(CLIENT)
	$(CLIENT)

test: $(TEST)
	$(TEST)

-include $(DEPS_SERVER)
-include $(DEPS_CLIENT)
-include $(DEPS_COMMON)
-include $(DEPS_TEST)
