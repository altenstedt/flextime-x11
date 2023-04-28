bin_PROGRAMS = flextimed flextime
flextimed_SOURCES = measurement.pb-c.h measurement.pb-c.c daemon.c
flextime_SOURCES = measurement.pb-c.h measurement.pb-c.c print.c

flextimed_LDADD = ${GIO_LIBS}
flextimed_CFLAGS = ${GIO_CFLAGS}

# https://stackoverflow.com/a/22178154
# https://www.gnu.org/software/automake/manual/html_node/Sources.html
# https://stackoverflow.com/a/18328947
BUILT_SOURCES = measurement.pb-c.c measurement.pb-c.h
MOSTLYCLEANFILES = measurement.pb-c.c measurement.pb-c.h
dist_noinst_DATA = measurement.proto flextime.1 flextimed.1
man_MANS = flextime.1 flextimed.1

measurement.pb-c.c measurement.pb-c.h: measurement.proto
	protoc-c --c_out=. $<
