CXX = g++ -std=c++17 -Wall -Wextra -Werror -Wpointer-arith -Wnon-virtual-dtor -Wno-error=unused-variable

DEBUGFLAGS += -g -DDEBUG -Og

object = thread_pool.cpp epoll_utility.cpp log.cpp sft.cpp main.cpp

LIB = -pthread

sft: $(object)
	$(CXX) $(object) $(LIB) -o sft -Ofast

test: $(object)
	$(CXX) $(object) $(LIB) -o test $(DEBUGFLAGS)

clean:
	rm -f sft test
