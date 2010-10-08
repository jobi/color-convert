PKG=glew glib-2.0 gdk-pixbuf-2.0
CFLAGS=-Wall -g `pkg-config --cflags $(PKG)`
LDFLAGS=`pkg-config --libs $(PKG)` -lglut

color-convert: color-convert.c
