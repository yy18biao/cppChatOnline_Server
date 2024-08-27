all: server client
server: server.cc main.pb.cc
	g++ -std=c++17 $^ -o $@ -lbrpc -lleveldb -lgflags -lssl -lcrypto -lprotobuf
client: client.cc main.pb.cc
	g++ -std=c++17 $^ -o $@ -lbrpc -lleveldb -lgflags -lssl -lcrypto -lprotobuf

%.pb.cc : %.proto
	protoc --cpp_out ./ $<