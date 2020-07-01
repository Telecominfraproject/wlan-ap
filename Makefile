.PHONY: all opensync purge

all:
	./dock-run.sh ./build.sh  $(TARGET)

opensync:
	./dock-run.sh make -j$(nproc) V=s -C openwrt package/feeds/opensync/opensync/clean
	./dock-run.sh make -j$(nproc) V=s -C openwrt package/feeds/opensync/opensync/compile TARGET=$(TARGET) OPENSYNC_SRC=$(shell pwd)
purge:
	cd openwrt && rm -rf * && rm -rf .*
	rm -rf ./example/build
	@echo Done
