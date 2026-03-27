CC = gcc
CFLAGS = -O2 -march=armv8-a+sve
TARGET = page_boundary_load
SRC = page_boundary_load.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
