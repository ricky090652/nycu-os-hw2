.PHONY: all clean

CC = riscv64-linux-gnu-gcc
CFLAGS = -static -Wall
TARGET = sched_demo_314581038
SRC = sched_demo_314581038.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) -lpthread

clean:
	rm -f $(TARGET)

make