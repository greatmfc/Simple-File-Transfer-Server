CXX = g++

STDVER = -std=c++17

DEBUGFLAGS = -g -DDEBUG -Og

WARMFLAGS = -Wall -Wextra -Werror -Wpointer-arith -Wnon-virtual-dtor -Wno-error=unused-variable -Wno-error=unused-parameter

object = thread_pool.cpp epoll_utility.cpp log.cpp sft.cpp main.cpp

output = sft

LIB = -pthread

sft: $(object)
	$(CXX) $(STDVER) $(WARMFLAGS) $(object) $(LIB) -o $(output) -Ofast

test: $(object)
	$(CXX) $(STDVER) $(WARMFLAGS) $(object) $(LIB) -o test $(DEBUGFLAGS)

install: $(output)
	install -m 755 $(output) /usr/bin/

clean:
	rm -f sft test
	rm -f /usr/bin/sft
