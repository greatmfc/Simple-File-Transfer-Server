CXX = g++

STDVER = -std=c++20

DEBUGFLAGS = -g -DDEBUG -fsanitize=address

WARMFLAGS = -Wall -Wextra -Wpointer-arith -Wnon-virtual-dtor

OPTFLAGS = -fno-rtti -O2 -march=native

object = src/main.cpp

output = sft.out

LIB = -lstdc++

.PHONY: clean

sft: $(object)
	$(CXX) $(STDVER) $(WARMFLAGS) $(object) $(LIB) -o $(output) $(OPTFLAGS)

testing: $(object)
	$(CXX) $(STDVER) $(WARMFLAGS) $(object) $(LIB) -o test.out $(DEBUGFLAGS)

install: $(output)
	install -m 755 $(output) /usr/bin/

clean:
	rm -f sft.out test.out
	rm -f /usr/bin/sft
