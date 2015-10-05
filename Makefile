CC=gcc
LDLIBS=-lncurses
NAME=mantra
CFLAGS=-g
OBJS=mantra.o draw.o win/win.o win/bookmarks.o win/pages.o win/helpbar.o

all: $(NAME)

$(NAME):

$(NAME): $(OBJS)

clean:
	rm *[.]o
