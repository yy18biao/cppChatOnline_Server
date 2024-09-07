all:test test2
test:test.cc
	g++ -std=c++17 $^ -o $@ -lspdlog -lfmt -lgflags -letcd-cpp-api -lcpprest -ljsoncpp -lelasticlient

test2:test2.cc
	g++ -std=c++17 $^ -o $@ -lspdlog -lfmt -lgflags -letcd-cpp-api -lcpprest

.PHONY:clean
clean:
	rm -rf test test2