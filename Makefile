PROGRAM := spft
OBJECTS_DIR := objects
CFLAGS += -Wall
X11_TOP := /usr/X11R6

-include Makefile.local

SOURCES := TermWindow.cpp Terminal.cpp History.cpp Line.cpp Run.cpp
SOURCES += Settings.cpp UTF8.cpp main.cpp

OBJECTS = $(foreach source,$(SOURCES),$(OBJECTS_DIR)/$(source:.cpp=.o))
OBJECTS_SUBDIRS = $(foreach dir,$(SUBDIRS),$(OBJECTS_DIR)/$(dir))

ifndef VERBOSE_MAKE
	QUIET := @
endif

all: $(PROGRAM)

X11_INCLUDES := $(X11_TOP)/include
X11_LIBS := $(X11_TOP)/lib

CPP := g++
CFLAGS += -MMD
CFLAGS += -g
CFLAGS += $(foreach switch,$(SWITCHES),-D$(switch))

CFLAGS += -std=c++11 -I$(X11_INCLUDES)
LINK_FLAGS += -L$(X11_LIBS) -lX11 -lXft -lutil

$(OBJECTS_DIR)/%.o: %.cpp
	@echo Compiling $<...
	$(QUIET) $(CPP) -c $< -g $(CFLAGS) -o $@

$(PROGRAM): $(OBJECTS_DIR) $(OBJECTS)
	@echo "Linking $@..."
	$(QUIET) $(CPP) $(filter-out $(OBJECTS_DIR),$^) -g $(LINK_FLAGS) -o $@
	@echo "---------------------------------------------"
	@echo

$(OBJECTS_DIR):
	@echo "Making $@..."
	$(QUIET) mkdir -p $(OBJECTS_DIR) $(OBJECTS_SUBDIRS)

-include $(OBJECTS_DIR)/*.d


.PHONY: runnit
runnit: $(PROGRAM)
	@./$(PROGRAM) $(RUN_ARGS)


