CXX=g++
CFLAGS= -g -O0 -Wall -fPIC -Wno-deprecated

SRC=-I../../include
INC=-L../../lib -lreactor -lprotobuf -pthread
OBJS= $(addsuffix .o, $(basename $(wildcard $(SRC)/*.cpp)))

all:
		$(CXX) -o server $(CFLAGS) echoMessage.pb.cc server.cpp $(SRC) $(INC)
		$(CXX) -o client $(CFLAGS) echoMessage.pb.cc client.cpp $(SRC) $(INC)

clean:
		-rm -f *.o server client

