SRC_C=$(wildcard *.c)
SRC_CPP=$(wildcard *.cpp)
OBJ_C=$(patsubst %.c, %.o, $(SRC_C))
OBJ_CPP=$(patsubst %.cpp, %.o, $(SRC_CPP))

PWD=$(shell pwd)

all: rdpt_adpt

CFLAGS=-I${PWD}/../ -g
CPPFLAGS=$(CFLAGS)

$(OBJ_C): $(SRC_C)
$(OBJ_CPP): $(SRC_CPP)

rdpt_adpt: $(OBJ_C) $(OBJ_CPP)
	g++ -o $@ $^ -lpthread

clean:
	rm -rf $(OBJ_C) $(OBJ_CPP) rdpt_adpt

$(OBJ):$(SRC)
