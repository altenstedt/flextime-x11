bin_PROGRAMS = flextimed flextime
flextimed_SOURCES = daemon.c measurement.pb-c.c

flextime_SOURCES = print.c measurement.pb-c.c

measurement.pb-c.c: measurement.proto
	protoc-c --c_out=. $<
