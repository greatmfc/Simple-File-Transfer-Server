CXX = g++ -std=c++17 -Wall -Wextra -Werror -Wpointer-arith -Wnon-virtual-dtor -Wno-error=unused-variable -Wno-error=unused-parameter

DEBUGFLAGS += -g -DDEBUG -Og

object = thread_pool.cpp epoll_utility.cpp log.cpp sft.cpp main.cpp

output = sft

LIB = -pthread

sft: $(object)
	$(CXX) $(object) $(LIB) -o $(output) -Ofast

test: $(object)
	$(CXX) $(object) $(LIB) -o test $(DEBUGFLAGS)

install: $(output)
	install -m 755 $(output) /usr/bin/

clean:
	rm -f sft test
	rm -f /usr/bin/sft
