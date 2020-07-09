bin_PROGRAMS = flextimed flextime
flextimed_SOURCES = measurement.pb-c.c daemon.c

flextime_SOURCES = measurement.pb-c.c print.c

measurement.pb-c.c: measurement.proto
	protoc-c --c_out=. $<
