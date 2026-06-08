CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu99
TARGET = web_server 
SRC = src/main.c src/server.c src/http_parser.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
