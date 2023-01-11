.PHONY: clean

object = ../client.cpp ../log.cpp test.cpp

test: $(object)
	g++ -std=c++17 -Wall -Wextra $(object) -o test -DDEBUG

clean:
	rm test
