all:main
main:main.cc
	g++ -std=c++17 $^ -o $@ /usr/local/lib/libjsoncpp.so.1.8.4 -lcurl -lcrypto -lspdlog -lfmt

.PHONY:clean
clean:
	rm -rf main