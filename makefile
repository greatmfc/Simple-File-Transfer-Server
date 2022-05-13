CXX ?= g++

DEBUGFLAGS += -g -fsanitize=address

object = sft.cpp main.cpp

sft: $(object)
	$(CXX) $(object) -o sft -O3

test: $(object)
	$(CXX) -DDEBUG $(object) -o test $(DEBUGFLAGS)

clean:
	rm -f sft test
