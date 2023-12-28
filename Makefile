build:
	mpic++ --std=c++17 -o tema3 tracker.cpp peer.cpp tema3.cpp -pthread -Wall

# DELETE THIS BEFORE UPLOADING THE HW, IS ONLY FOR TESTING
run: build
	mpirun --oversubscribe -np 3 tema3

clean:
	rm -rf tema3
