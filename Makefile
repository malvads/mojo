UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    CXX = clang++
    INCLUDES += -I/opt/homebrew/include -I/usr/local/include
    LDFLAGS = -L/opt/homebrew/lib -L/usr/local/lib $(LIBS)
else
    CXX = g++
    INCLUDES += -I/usr/local/include
    LDFLAGS = $(LIBS)
endif

VERSION := $(shell cat VERSION)
CXXFLAGS += -DMOJO_VERSION=\"$(VERSION)\" $(INCLUDES)

SRC_DIR = src
OBJ_DIR = obj

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
