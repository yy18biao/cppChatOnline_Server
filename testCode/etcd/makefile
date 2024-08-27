all:etcdPutTest etcdGetTest

etcdPutTest:etcdPutTest.cc
	g++ -std=c++17 $^ -o $@ -letcd-cpp-api -lcpprest

etcdGetTest:etcdGetTest.cc
	g++ -std=c++17 $^ -o $@ -letcd-cpp-api -lcpprest