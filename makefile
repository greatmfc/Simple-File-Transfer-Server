CXX ?= g++

DEBUGFLAGS += -g -fsanitize=address

object = sft.cpp main.cpp

sft: $(object)
	$(CXX) $(object) -o sft

test: $(object)
	$(CXX) $(object) -o test $(DEBUGFLAGS)

clean:
	rm -f sft
