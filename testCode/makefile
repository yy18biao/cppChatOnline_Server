all:test test2
test:test.cc
	g++ -std=c++17 $^ -o $@ -lspdlog -lfmt -lgflags -letcd-cpp-api -lcpprest -ljsoncpp -lelasticlient -L /usr/lib/ -lamqpcpp -lev -lpthread -ldl

test2:test2.cc
	g++ -std=c++17 $^ -o $@ -lspdlog -lfmt -lgflags -letcd-cpp-api -lcpprest -ljsoncpp -lelasticlient -L /usr/lib/ -lamqpcpp -lev -lpthread -ldl

.PHONY:clean
clean:
	rm -rf test test2