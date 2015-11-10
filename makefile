CC=gcc
TARGET=uds

# single file, so intermediate .o files not needed
all: $(TARGET).out

$(TARGET).out: $(TARGET).c
	$(CC) $(TARGET).c -o $(TARGET).out

clean: 
	rm -rf $(TARGET).out

run: all
	./$(TARGET).out