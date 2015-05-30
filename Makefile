NAME    := asciigram
FLAGS   := -lcurl -ljansson -lcurses -ljpeg -lm
BIN_DIR := /usr/local/bin

all: build

build: $(NAME).c
	gcc -Wall -Wextra -o $(NAME) $< $(FLAGS)

install: $(NAME)
	install -m 0755 $(NAME) $(BIN_DIR)

uninstall:
	rm -f $(BIN_DIR)/$(NAME)

clean:
	rm -f $(NAME)