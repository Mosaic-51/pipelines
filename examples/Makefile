CXXFILES = test1.cpp ../pipeline.cpp
OFILES = ${CXXFILES:.cpp=.o}

CXXFLAGS = -std=c++17 -Wall -Werror -Wextra -Og

all: test1

test1: ${OFILES}
	${CXX} ${CXXFLAGS} $? -o $@

