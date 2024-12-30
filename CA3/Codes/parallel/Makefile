CXX = g++
CXXFLAGS = -Wall -std=c++11
LDFLAGS = -lsndfile

all: VoiceFilters

VoiceFilters: main.o
	$(CXX) -o $@ $^ $(LDFLAGS)

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f *.o VoiceFilters
