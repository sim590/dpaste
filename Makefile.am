
dpaste_CPPFLAGS_ = ${GLIBMM_CFLAGS} ${CURLPP_CLFAGS} ${GPGME_CFLAGS}
dpaste_LIBS      = ${OpenDHT_LIBS} ${GLIBMM_LIBS} ${CURLPP_LIBS} -lb64 -lgpgmepp ${GPGME_LIBS}
export

SUBDIRS = src
if DPASTE_TEST
SUBDIRS += tests
endif

dist_man1_MANS = doc/dpaste.1

ACLOCAL_AMFLAGS = -I m4

DOC_FILES = \
			README.md \
			config/dpaste.conf \
			COPYING

EXTRA_DIST = \
			$(DOC_FILES)

test: check
if DPASTE_TEST
	./tests/dptest $(DPTEST_ARGS)
endif

#  vim: set ts=4 sw=4 tw=120 noet :

