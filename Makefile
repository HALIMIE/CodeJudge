.PHONY: all configure clean
all:
	$(MAKE) -C build

configure:
	@mkdir -p build
	cd build && cmake ..

clean:
	rm -rf build
	@mkdir -p build
	cd build && cmake ..

