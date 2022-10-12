CXXFILES = test1.cpp ../pipeline.cpp
OFILES = ${CXXFILES:.cpp=.o}

CFLAGS += -std=c++17
CFLAGS += -Werror -Wall -Wextra -std=c99 -march=native
CFLAGS += -fsanitize=undefined -fsanitize=address
CFLAGS += -Og -g

all: test1

test1: ${OFILES}
	${CXX} ${CFLAGS} -o $@ $^

