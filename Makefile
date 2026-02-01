CXX = clang++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -Wno-delete-non-abstract-non-virtual-dtor -Wno-unused-parameter
INCLUDES = -Iinclude -Iinclude/vendor

# OS Detection
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    # macOS (Homebrew / Local)
    INCLUDES += -I/opt/homebrew/include -I/usr/local/include
    LDFLAGS = -L/opt/homebrew/lib -L/usr/local/lib -lcurl -lgumbo
else
    # Linux (Standard)
    LDFLAGS = -lcurl -lgumbo
endif

VERSION := $(shell cat VERSION)
CXXFLAGS += -DMOJO_VERSION=\"$(VERSION)\" $(INCLUDES)

SRC_DIR = src
OBJ_DIR = obj

# Find generic and vendor sources
SRCS = $(wildcard $(SRC_DIR)/*.cpp) $(wildcard $(SRC_DIR)/vendor/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

TARGET = mojo

all: directories $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

directories:
	@mkdir -p $(OBJ_DIR)/vendor

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean directories
