CROSS = arm-linux-gnueabihf-
TARGET = ktimer
SRC = ktimer.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CROSS)gcc -Os -static -s -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
