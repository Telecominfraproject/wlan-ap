.PHONY: all purge

all:
	./dock-run.sh ./build.sh  $(TARGET) $(SDK_URL) $(PROFILE) $(IMAGE_URL)

purge:
	rm -rf ./build
	rm -rf ./example/build
	@echo Done
