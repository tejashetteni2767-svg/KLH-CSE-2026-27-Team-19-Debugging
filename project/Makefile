CC = gcc
CFLAGS = -Wall -Wextra -g -O0 -Iinclude
GTK_FLAGS = $(shell pkg-config --cflags --libs gtk+-3.0)

all: test_program debugger gui

test_program:
	$(CC) $(CFLAGS) test/test_program.c -o bin/test_program

debugger:
	$(CC) $(CFLAGS) src/debugger.c src/main.c -o bin/my_debugger

gui:
	$(CC) $(CFLAGS) gui/gui.c src/debugger.c -o bin/debugger_gui $(GTK_FLAGS)

clean:
	rm -f bin/test_program bin/my_debugger bin/debugger_gui

run: all
	./bin/debugger_gui
