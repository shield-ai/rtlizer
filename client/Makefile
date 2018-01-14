#
# 'make'        build executable file 'rtlizer'
# 'make clean'  removes all .o and executable files
#

CC = gcc
CFLAGS = -Wall -O3 `pkg-config --cflags gtk+-3.0`
LIBS = -lm `pkg-config --libs gtk+-3.0`

#INCLUDES = -I./src/
#LFLAGS = 

# C source files
SRCS = client.c

OBJS = $(SRCS:.c=.o)

MAIN = rtlizer_client

# generic makefile
.PHONY: depend clean

all:    $(MAIN)

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)

