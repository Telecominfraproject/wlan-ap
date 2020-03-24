.PHONY: all purge docker_console start_qemu

all:
	cd .. && ./example/dock-run.sh  ./example/build.sh  $(TARGET) $(SDK_URL)

purge:
	rm -rf ./build
	@echo Done

docker_console:
	cd .. && ./example/dock-run.sh

start_qemu:
	sudo ./qemu/run.sh $(IMAGE_URL)
