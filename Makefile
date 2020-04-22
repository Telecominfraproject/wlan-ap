.PHONY: all purge docker_console start_qemu

all:
	./build.sh  $(TARGET) $(SDK_URL) $(PROFILE) $(IMAGE_URL)

purge:
	rm -rf ./build
	@echo Done
