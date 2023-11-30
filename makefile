# Compiler settings
CXX = g++
CXXFLAGS = -Wall -std=c++17

all: servermain serverA serverB serverC student admin

servermain: servermain.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

serverA: server.cpp
	$(CXX) $(CXXFLAGS) -DBACKEND_SERVER_NAME=SERVER_A $^ -o $@

serverB: server.cpp
	$(CXX) $(CXXFLAGS) -DBACKEND_SERVER_NAME=SERVER_B $^ -o $@

serverC: server.cpp
	$(CXX) $(CXXFLAGS) -DBACKEND_SERVER_NAME=SERVER_C $^ -o $@

student: student.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

admin: admin.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

runA: serverA
	./serverA

runB: serverB
	./serverB

runC: serverC
	./serverC

runMain: servermain
	./servermain

runStudent1: student
	./student

runStudent2: student
	./student

runAdmin: admin
	./admin

clean:
	rm -f servermain serverA serverB serverC

