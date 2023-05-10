CXX = g++

STDVER = -std=c++20

DEBUGFLAGS = -g -DDEBUG -fsanitize=address

WARMFLAGS = -Wall -Wextra -Wpointer-arith -Wnon-virtual-dtor

object = src/main.cpp

output = sft.out

LIB = -lstdc++

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
