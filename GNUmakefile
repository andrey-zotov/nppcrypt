TARGET := nppcrypt
OBJDIR := obj
SRCDIR := src
CXX := g++
C := gcc
CXXFLAGS := -std=c++11
CFLAGS := 
# cryptopp is built in src/cryptopp (instead of obj/cryptopp and bin/cryptopp) to avoid having to mess with the cryptopp makefile
CRYPTOPP := src/cryptopp/libcryptopp.a
LDFLAGS := -lstdc++ -Lsrc/cryptopp -lcryptopp
PREFIX := /usr/local
unexport LDFLAGS

DEP_SRC := $(shell find $(SRCDIR)/bcrypt -type f -name *.cpp)
DEP_SRC += $(shell find $(SRCDIR)/scrypt -type f -name *.c)
DEP_SRC += $(shell find $(SRCDIR)/keccak -type f -name *.cpp)
DEP_SRC += $(shell find $(SRCDIR)/tinyxml2 -type f -name *.cpp)
MAIN_SRC := src/clihelp.cpp src/crypt_help.cpp src/crypt.cpp src/cmdline.cpp src/exception.cpp src/cryptheader.cpp

ifeq ($(mode),debug)
	CFLAGS += -g3 -ggdb -O0 -Wall -Wextra -Wno-unused -DDEBUG
	CXXFLAGS += -g3 -ggdb -O0 -Wall -Wextra -Wno-unused -DDEBUG
	SUBDIR := debug
else
	CFLAGS += -g2 -Os -fdata-sections -ffunction-sections -DNDEBUG
	CXXFLAGS += -g2 -Os -fdata-sections -ffunction-sections -DNDEBUG
	LDFLAGS += 
	SUBDIR := release
endif

DEP_OBJ := $(patsubst $(SRCDIR)/%,$(OBJDIR)/$(SUBDIR)/%,$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(DEP_SRC))))
MAIN_OBJ := $(patsubst $(SRCDIR)/%,$(OBJDIR)/$(SUBDIR)/%,$(MAIN_SRC:.cpp=.o))

.PHONY: all
all:
	$(MAKE) info
	$(MAKE) directories
	$(MAKE) $(CRYPTOPP)
	$(MAKE) bin/$(SUBDIR)/$(TARGET)

.PHONY: info
info:
ifeq ($(mode),debug)
	@echo "building nppcrypt in debug mode..."
else
	@echo "building nppcrypt in release mode..."
endif

.PHONY: directories
directories:
	@mkdir -p bin/$(SUBDIR)
	@mkdir -p obj/$(SUBDIR)
	@mkdir -p obj/$(SUBDIR)/bcrypt
	@mkdir -p obj/$(SUBDIR)/scrypt
	@mkdir -p obj/$(SUBDIR)/keccak
	@mkdir -p obj/$(SUBDIR)/tinyxml2

.PHONY: clean
clean:
	@make -C src/cryptopp clean
	@rm -rf $(OBJDIR)

.PHONY: install
install: bin/release/$(TARGET)
ifeq ($(target),global)
	@cp $< /usr/bin/$(TARGET)
else
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@cp $< $(DESTDIR)$(PREFIX)/bin/$(TARGET)
endif

.PHONY: uninstall
uninstall:
ifeq ($(target),global)
	@rm -f /usr/bin/$(TARGET)
else
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
endif

$(CRYPTOPP):
	$(MAKE) -C src/cryptopp

bin/$(SUBDIR)/$(TARGET): $(MAIN_OBJ) $(DEP_OBJ)
	$(CXX) $(CXXFLAGS) -o bin/$(SUBDIR)/$(TARGET) $^ $(LDFLAGS)

$(OBJDIR)/$(SUBDIR)/scrypt/%.o: src/scrypt/%.c
	$(C) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/$(SUBDIR)/bcrypt/%.o: src/bcrypt/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/$(SUBDIR)/keccak/%.o: src/keccak/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/$(SUBDIR)/tinyxml2/%.o: src/tinyxml2/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/$(SUBDIR)/crypt.o: src/crypt.cpp 
	$(CXX) $(CXXFLAGS) -DCRYPTOPP_DISABLE_ASM -DCRYPTOPP_DISABLE_MIXED_ASM -c -o $@ $<

$(OBJDIR)/$(SUBDIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

