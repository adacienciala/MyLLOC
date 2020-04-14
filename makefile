CC	= gcc
LIBS	= -pthread
ODIR	= obj
DEPS	= mylloc.h custom_unistd.h
_OBJ	= main.o mylloc.o custom_unistd.o
OBJ	= $(patsubst %,$(ODIR)/%,$(_OBJ))
TARGET	= mylloc

$(ODIR)/%.o: %.c $(DEPS)
	@mkdir -p $(ODIR)
	$(CC) -c -o $@ $<

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LIBS) 

clean:
	rm -r $(ODIR) $(TARGET) 