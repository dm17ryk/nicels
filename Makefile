.PHONY: all debug release windows package clean

all: release

configure-debug:
	cmake --preset linux-debug

configure-release:
	cmake --preset linux-release

build-debug:
	cmake --build --preset linux-debug

build-release:
	cmake --build --preset linux-release

release: configure-release build-release

debug: configure-debug build-debug

windows:
	cmake --preset windows-ucrt
	cmake --build --preset windows-release

package:
	cmake --build --preset linux-release --target package

clean:
	rm -rf obj bin build _CPack_Packages CMakeCache.txt CMakeFiles
