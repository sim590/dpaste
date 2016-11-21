bin_PROGRAMS = dpaste pasted

dpaste_SOURCES = \
				 dpaste.cpp \
				 node.cpp
pasted_SOURCES = \
				 pasted.cpp \
				 server.cpp \
				 ipc.cpp \
				 node.cpp

lflags = -lopendht -lgnutls -lnettle
dpaste_LDFLAGS = $(lflags)
pasted_LDFLAGS = $(lflags) -lpthread
