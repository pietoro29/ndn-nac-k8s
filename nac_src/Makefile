CXX = g++
CXXFLAGS = -std=c++17 -Wall -o2 $(shell pkg-config --cflags libndn-cxx libndn-nac)
LDFLAGS = $(shell pkg-config --libs libndn-cxx libndn-nac)

TARGETS = consumer producer kdk-server
all: $(TARGETS)

consumer: consumer.o
	$(CXX) -o $@ $^ $(LDFLAGS)

producer: producer.o
	$(CXX) -o $@ $^ $(LDFLAGS)

kdk-server: kdk-server.o
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp nac-utils.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGETS)

.PHONY: all clean
