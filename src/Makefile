PACKAGE = dpaste

CC = g++
CFLAGS = -c -std=c++11
LFLAGS = -lgnutls -lopendht

$(PACKAGE): main.o node.o
	$(CC) -o $(PACKAGE) $^ $(LFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) $^

clean:
	rm -rf *.o $(PACKAGE)
