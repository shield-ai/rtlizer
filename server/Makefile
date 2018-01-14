#
# 'make'        build executable file 'rtlizer'
# 'make clean'  removes all .o and executable files
#

CC = gcc
CFLAGS = -Wall -O3 `pkg-config --cflags librtlsdr`
LIBS = -lm `pkg-config --libs librtlsdr`

#INCLUDES = -I./src/
#LFLAGS = 

# C source files
SRCS = kiss_fft.c server.c

OBJS = $(SRCS:.c=.o)

MAIN = rtlizer_server

# generic makefile
.PHONY: depend clean

all:    $(MAIN)

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)

