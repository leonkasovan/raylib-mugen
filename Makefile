# ==============================================
# Auto OS detection
# ==============================================
ifeq ($(OS),Windows_NT)
	EXE_EXT = .exe
	PLATFORM = windows
	RM = del /Q
else
	EXE_EXT =
	PLATFORM = linux
	RM = rm -f
endif

# ==============================================
# Compiler and flags
# ==============================================
CXX = g++
SRC = main.cpp lodepng.cpp
OBJ = $(SRC:.cpp=.o)

# ==============================================
# Common settings
# ==============================================
INCLUDES = -I../raylib/src
LINUX_LIBS = -L../raylib/src -lraylib -lEGL -ldrm -lgbm -lGLESv2
WIN_LIBS   = -lraylib -lgdi32 -lwinmm

# ==============================================
# Build type settings
# ==============================================
DEBUG_FLAGS = -O1 -g -Wall -Wextra -fno-omit-frame-pointer -fsanitize=address
DEBUG_LDFLAGS = -fsanitize=address
RELEASE_FLAGS = -O2 -DNDEBUG -Wall -Wextra
RELEASE_LDFLAGS = 

# ==============================================
# Targets
# ==============================================
.PHONY: all debug release clean

all: release

# --- Debug build ---
debug: CXXFLAGS = $(DEBUG_FLAGS)
debug: LDFLAGS = $(DEBUG_LDFLAGS)
debug: TARGET = apps_debug$(EXE_EXT)
debug: build

# --- Release build ---
release: CXXFLAGS = $(RELEASE_FLAGS)
release: LDFLAGS = $(RELEASE_LDFLAGS)
release: TARGET = apps_release$(EXE_EXT)
release: build

# --- Build rule ---
build: $(OBJ)
	@echo "Building $(TARGET) for $(PLATFORM)"
ifeq ($(PLATFORM),linux)
	$(CXX) $(OBJ) -o $(TARGET) $(INCLUDES) $(LINUX_LIBS) $(LDFLAGS)
else
	$(CXX) $(OBJ) -o $(TARGET) $(INCLUDES) $(WIN_LIBS) $(LDFLAGS)
endif

# --- Object build rule ---
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# --- Cleanup ---
clean:
	@echo "Cleaning up..."
	-$(RM) *.o apps_debug$(EXE_EXT) apps_release$(EXE_EXT)
