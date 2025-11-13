#taken from lab3/ul/Makefile

CC = gcc
TARGET = myISS
SRC = myiss.c

# useful flags: https://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Option-Summary.html#Option-Summary
# more ref for optimization: https://www.reddit.com/r/C_Programming/comments/wfesjj/what_does_marchnative_do/

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -O3 -march=native -mtune=native -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
