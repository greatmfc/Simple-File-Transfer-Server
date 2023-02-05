CXX = g++

STDVER = -std=c++2a

DEBUGFLAGS = -g -DDEBUG

WARMFLAGS = -Wall -Wextra -Wpointer-arith -Wnon-virtual-dtor \
			-Wno-error=unused-variable -Wno-error=unused-parameter

object = thread_pool.cpp epoll_utility.cpp log.cpp server.cpp client.cpp main.cpp

output = sft.out

LIB = -pthread

.PHONY: clean

sft: $(object)
	$(CXX) $(STDVER) $(WARMFLAGS) $(object) $(LIB) -o $(output) -O3 -march=native

testing: $(object)
	$(CXX) $(STDVER) $(WARMFLAGS) $(object) $(LIB) -o test.out $(DEBUGFLAGS)

install: $(output)
	install -m 755 $(output) /usr/bin/

clean:
	rm -f sft.out test.out
	rm -f /usr/bin/sft
