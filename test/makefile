.PHONY: clean

object = ../client.cpp test.cpp

test: $(object)
	g++ -std=c++17 -Wall -Wextra $(object) -o test -Ofast

clean:
	rm test
