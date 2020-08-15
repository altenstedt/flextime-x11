bin_PROGRAMS = flextimed flextime
flextimed_SOURCES = measurement.pb-c.h measurement.pb-c.c daemon.c
flextime_SOURCES = measurement.pb-c.h measurement.pb-c.c print.c

# https://stackoverflow.com/a/22178154
# https://www.gnu.org/software/automake/manual/html_node/Sources.html
# https://stackoverflow.com/a/18328947
BUILT_SOURCES = measurement.pb-c.c measurement.pb-c.h
MOSTLYCLEANFILES = measurement.pb-c.c measurement.pb-c.h
dist_noinst_DATA = measurement.proto

measurement.pb-c.c measurement.pb-c.h: measurement.proto
	protoc-c --c_out=. $<
