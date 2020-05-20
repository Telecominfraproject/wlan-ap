.PHONY: all clean purge

all:
	./dock-run.sh ./build.sh  $(TARGET) $(SDK_URL)

purge:
	cd openwrt && rm -rf * && rm -rf .*
	rm -rf ./example/build
	@echo Done
