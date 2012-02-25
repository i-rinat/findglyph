findglyph: scan.cc
	g++ -Wall -O2 -pipe scan.cc -o findglyph `pkg-config --cflags --libs freetype2 glibmm-2.4 sqlite3`

all: findglyph


