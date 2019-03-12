all: world

INCLUDE_DIRS= /usr/local/include \
LIBRARY_DIRS= /usr/lib64  \

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(dir $(mkfile_path))

world: o2property o2logger
	make ROOT_SRC=${current_dir} -C server/src/
	make ROOT_SRC=${current_dir} -C client/src/

o2property:
	make ROOT_SRC=${current_dir} -C third_party/libproperty

o2logger:
	make ROOT_SRC=${current_dir} -C third_party/o2logger

clean:
	rm -rf client/src/BUILD
	rm -rf server/src/BUILD

.PHONY: world clean libproperty o2logger
