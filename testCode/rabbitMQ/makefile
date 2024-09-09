all:p c
p:p.cc
	g++ -std=c++17 $^ -o $@ -L /usr/lib/ -lamqpcpp -lev -lpthread -ldl

c:c.cc
	g++ -std=c++17 $^ -o $@ -L /usr/lib/ -lamqpcpp -lev -lpthread -ldl

.PHONY:clean
clean:
	rm -rf p c