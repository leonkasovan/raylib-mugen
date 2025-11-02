all: linux

linux:
	@echo "Building for Linux DRM"
	g++ -o apps -DGRAPHICS_API_OPENGL_ES3 -I../raylib/src -L../raylib/src main.cpp lodepng.cpp -lraylib -lEGL -ldrm -lgbm -lGLESv2

win64:
	@echo "Building for Win64"
	g++ -o apps.exe main.cpp lodepng.cpp -lraylib -lgdi32 -lwinmm