# Covid Backtracer.
#
# For now, all-in-one binary; this is meant to be split at some point.

CXXFLAGS = -g -Wall -std=c++17 $(shell freetype-config --cflags) -Isrc
LDLIBS = -lglog -lgflags

PRGM := bin/bt
TEST := bin/bt_test
CXX  := clang++
FMT  := clang-format

SRCS := $(filter-out $(wildcard src/*test.cc), $(wildcard src/*.cc))
OBJS := $(SRCS:.cc=.o)
DEPS := $(OBJS:.o=.d)

SRCS_TEST := $(filter-out src/main.cc, $(wildcard src/*.cc))
OBJS_TEST := $(SRCS_TEST:.cc=.o)
DEPS_TEST := $(OBJS_TEST:.o=.d)

.PHONY: all clean re test fmt

all: $(PRGM)

fmt:
	$(FMT) -i -style Chromium $(SRCS)

clean:
	rm -rf $(OBJS) $(DEPS) $(PRGM)
	rm -rf $(OBJS_TEST) $(DEPS_TEST) $(TEST)

bin:
	mkdir -p bin

re: clean all

$(PRGM): bin $(OBJS)
	$(CXX) $(OBJS) $(LDLIBS) -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(TEST): bin $(OBJS_TEST)
	$(CXX) $(OBJS_TEST) $(LDLIBS) -lgtest -o $@

test: $(TEST)
	$(TEST)

-include $(DEPS)
-include $(DEPS_TEST)
