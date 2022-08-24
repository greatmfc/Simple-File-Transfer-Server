CXX = g++

CXX12 = g++-12

STDVER = -std=c++17

DEBUGFLAGS = -g -DDEBUG -Og

WARMFLAGS = -Wall -Wextra -Werror -Wpointer-arith -Wnon-virtual-dtor \
			-Wno-error=unused-variable -Wno-error=unused-parameter \
			-Wno-error=stringop-overflow

object = thread_pool.cpp epoll_utility.cpp log.cpp sft.cpp main.cpp

output = sft

LIB = -pthread

.PHONY: clean

sft12: $(object)
	$(CXX12) $(STDVER) -fuse-ld=mold $(WARMFLAGS) $(object) $(LIB) -o $(output) -Ofast

sft: $(object)
	$(CXX) $(STDVER) $(WARMFLAGS) $(object) $(LIB) -o $(output) -Ofast

test: $(object)
	$(CXX) $(STDVER) $(WARMFLAGS) $(object) $(LIB) -o test $(DEBUGFLAGS)

install: $(output)
	install -m 755 $(output) /usr/bin/

clean:
	rm -f sft test
	rm -f /usr/bin/sft
