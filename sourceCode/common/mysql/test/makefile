main : main.cc message-odb.cxx
	g++ -std=c++17 $^ -o $@  -I../../../odb/ -I./ -lodb-mysql -lodb -lodb-boost -lfmt -lspdlog -lgflags