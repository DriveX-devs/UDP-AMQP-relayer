EXECNAME=UDPAMQPrelayer

SRC_DIR=src
OBJ_DIR=obj

OBJ_RAWSOCK_DIR=obj/Rawsock_lib

SRC=$(wildcard $(SRC_DIR)/*.cpp)

OBJ=$(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

OBJ_CC=$(OBJ)

CXXFLAGS += -Wall -O3 -Iinclude -IRawsock_lib/Rawsock_lib -I.
CFLAGS += -Wall -O3 -IRawsock_lib/Rawsock_lib
LDLIBS += -lpthread -lqpid-proton-cpp

.PHONY: all clean

all: compilePC

compilePC: CXX = g++
compilePC: CC = gcc
	
compilePCdebug: CXXFLAGS += -g
compilePCdebug: compilePC

compilePC compilePCdebug: $(EXECNAME)

# Standard targets
$(EXECNAME): $(OBJ_CC)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) $(CXXFLAGS) $(CFLAGS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@ mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ_DIR)/*.o $(OBJ_RAWSOCK_DIR)/*.o
	-rm -rf $(OBJ_DIR)
	-rm -rf $(OBJ_RAWSOCK_DIR)
	
fullclean: clean
	$(RM) $(EXECNAME)
