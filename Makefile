CC = gcc
CFLAGS = -std=c89 -Wall -O2

TARGET = yay0tool
SRCS = yay0.c main.c
OBJS = $(SRCS:.c=.o)

TEST_TARGET = yay0tool_test
TEST_SRCS = yay0.c test.c
TEST_OBJS = $(TEST_SRCS:.c=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_OBJS) $(TEST_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)
