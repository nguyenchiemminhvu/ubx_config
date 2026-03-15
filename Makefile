# ─────────────────────────────────────────────────────────────────────────────
# Makefile — UBX configuration library
# Target: Linux / POSIX, C++14
# ─────────────────────────────────────────────────────────────────────────────

CXX      ?= g++
CXXFLAGS  = -std=c++14 -Wall -Wextra -Wpedantic -I.
AR       ?= ar
ARFLAGS   = rcs

# ── Sources ───────────────────────────────────────────────────────────────────

LIB_SRCS = \
    ubx_cfg_valset_builder.cpp    \
    ubx_cfg_valget_builder.cpp    \
    ubx_config_repository.cpp     \
    ubx_cfg_key_registry.cpp      \
    ubx_config_sync_service.cpp   \
    ubx_config_manager.cpp

LIB_OBJS  = $(LIB_SRCS:.cpp=.o)
LIB_NAME  = libubx_config.a

EXAMPLE_SRC  = example_usage.cpp
EXAMPLE_BIN  = example_usage

# ── Default target ────────────────────────────────────────────────────────────

.PHONY: all clean

all: $(LIB_NAME)

# ── Library ───────────────────────────────────────────────────────────────────

$(LIB_NAME): $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ── Example ───────────────────────────────────────────────────────────────────

example: $(LIB_NAME) $(EXAMPLE_SRC)
	$(CXX) $(CXXFLAGS) $(EXAMPLE_SRC) -L. -lubx_config -o $(EXAMPLE_BIN)

# ── Clean ─────────────────────────────────────────────────────────────────────

clean:
	$(RM) $(LIB_OBJS) $(LIB_NAME) $(EXAMPLE_BIN)
