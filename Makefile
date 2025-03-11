CC = mpic++
CFLAGS = -Wall -Werror 
LDFLAGS = -pthread
SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)

build: $(OBJS)
	$(CC) $(CFLAGS) -o mpi-bittorrent $^ $(LDFLAGS)

clean:
	rm -rf mpi-bittorrent $(OBJS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<