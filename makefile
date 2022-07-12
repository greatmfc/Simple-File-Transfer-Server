CXX = g++ -Wall -Wextra -Werror -Wpointer-arith -Wnon-virtual-dtor -std=c++17

DEBUGFLAGS += -g -DDEBUG -O0

object = epoll_utility.cpp log.cpp sft.cpp main.cpp

LIB = -pthread

sft: $(object)
	$(CXX) $(object) $(LIB) -o sft -O3

test: $(object)
	$(CXX) $(object) $(LIB) -o test $(DEBUGFLAGS)

clean:
	rm -f sft test
