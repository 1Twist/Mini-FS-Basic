# ------------------------ CONFIGURÁVEIS ---------------------------------
CC      ?= gcc
CSTD      = gnu17   
WARNINGS  = -Wall -Wextra -Wpedantic
DEBUG     = -g
INC_DIRS  = -Iinclude
GLIB_CFLG = $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS = $(shell pkg-config --libs   glib-2.0)
CFLAGS    = $(WARNINGS) $(DEBUG) -std=$(CSTD) $(INC_DIRS) $(GLIB_CFLG)
LDFLAGS   = $(GLIB_LIBS)

SRC_DIR   = src
OBJ_DIR   = build
SRCS      = $(wildcard $(SRC_DIR)/*.c)
OBJS      = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

TARGET    = mfs                     # binário final
# ------------------------------------------------------------------------

.PHONY: all clean run

all: $(TARGET)

# regra final de link
$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

# regra de objeto com auto-dependências
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# cria pasta build se não existir
$(OBJ_DIR):
	mkdir -p $@

# inclui *.d (dependências geradas pelo -MMD)
-include $(OBJS:.o=.d)

# conveniência
run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
