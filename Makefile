.PHONY: all purge

all:
	./dock-run.sh ./build.sh  $(TARGET)

purge:
	cd openwrt && rm -rf * && rm -rf .*
	@echo Done
