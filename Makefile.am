bin_PROGRAMS = flextime
flextime_SOURCES = main.c measurement.pb-c.c

measurement.pb-c.c: measurement.proto
	protoc-c --c_out=. $<
