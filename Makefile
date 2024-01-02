build:
	mpic++ --std=c++17 -o tema3 tracker.cpp peer.cpp tema3.cpp -pthread -Wall

clean:
	rm -rf tema3
