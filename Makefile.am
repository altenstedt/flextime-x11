bin_PROGRAMS = flextimed
flextimed_SOURCES = daemon.c measurement.pb-c.c

measurement.pb-c.c: measurement.proto
	protoc-c --c_out=. $<
