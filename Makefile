# Covid Backtracer.
#
# For now, all-in-one binary; this is meant to be split at some point.

CXXFLAGS = -g -Wall -std=c++17 $(shell freetype-config --cflags) -Isrc -I.
LDLIBS = -lglog -lgflags -lrocksdb -lboost_filesystem

PRGM := bin/bt
TEST := bin/bt_test
CXX  := clang++
FMT  := clang-format
PBUF := protoc

SRCS := $(filter-out $(wildcard src/*test.cc), $(wildcard src/*.cc))
OBJS := $(SRCS:.cc=.o)
DEPS := $(OBJS:.o=.d)

SRCS_TEST := $(filter-out src/main.cc, $(wildcard src/*.cc))
OBJS_TEST := $(SRCS_TEST:.cc=.o)
DEPS_TEST := $(OBJS_TEST:.o=.d)

SRCS_PB   := $(wildcard proto/*.proto)
OBJS_PB   := $(SRCS_PB:.proto=.pb.o)
GENS_PB   := $(SRCS_PB:.proto=.pb.cc) $(PROTOS:.proto=.pb.h)

.PHONY: all clean re test fmt help run inject

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
	@echo "make run	# run a local instance of covid backtracer"
	@echo "make inject	# inject fixtures into local instance"
	@echo ""

all: $(PRGM) $(TEST)

fmt:
	$(FMT) -i -style Chromium $(SRCS)

clean:
	rm -rf $(OBJS) $(DEPS) $(PRGM)
	rm -rf $(OBJS_TEST) $(DEPS_TEST) $(TEST)
	rm -rf $(GENS_PB) $(OBJS_PB)

bin:
	mkdir -p bin

re: clean all

$(PRGM): bin $(OBJS) $(OBJS_PB)
	$(CXX) $(OBJS) $(LDLIBS) -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

%.pb.cc: %.proto
	$(PBUF) --cpp_out=. $<

%.pb.o : %.pb.cc
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

run: $(PRGM)
	$(PRGM)

inject: $(PRGM)

$(TEST): bin $(OBJS_TEST) $(OBJS_PB)
	$(CXX) $(OBJS_TEST) $(LDLIBS) -lgtest -o $@

test: $(TEST)
	$(TEST)

-include $(DEPS)
-include $(DEPS_TEST)
