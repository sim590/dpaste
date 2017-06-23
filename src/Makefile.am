bin_PROGRAMS = dpaste

dpaste_SOURCES = \
				 main.cpp \
				 node.cpp \
				 conf.cpp

dpaste_CPPFLAGS = $(GLIBMM_CFLAGS) @Cpr_CFLAGS@
dpaste_LDFLAGS  = @Cpr_LDFLAGS@
dpaste_LDADD  = -lopendht -lgnutls -lnettle $(GLIBMM_LIBS) -lcpr -lcurl -lb64

#########
#  cpr  #
#########

dpaste_DEPENDENCIES = libcpr.a

cpr_path = $(top_builddir)/cpr
cpr_build_path = $(top_builddir)/cpr/build

libcpr.a: $(cpr_build_path)/lib/libcpr.a
$(cpr_build_path)/lib/libcpr.a:
	$(shell mkdir $(cpr_build_path))
	cd $(cpr_build_path) && cmake .. && make

clean-local:
	rm -rf $(cpr_build_path)

