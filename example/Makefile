.PHONY: all opensync purge

all:
	cd .. && ./example/build.sh  $(TARGET) $(SDK_URL)

opensync:
	cd .. && ./dock-run.sh  ./example/build.sh  $(TARGET) $(SDK_URL)

purge:
	rm -rf ./build
	rm -rf ./out/*
	@echo Done
