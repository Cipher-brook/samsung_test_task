# The makefile builds test application to test kernel module

SRC=$(wildcard *.c)
CFLAGS = -O2 -Wall -Wpedantic -Wextra -Werror -I../common -fstack-protector

.PHONY: all
all: test_app

.PHONY: test_app
test_app: $(SRC)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean
clean:
	$(RM) test_app
