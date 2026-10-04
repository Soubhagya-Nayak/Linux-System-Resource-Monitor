CXX ?= g++
CC ?= gcc
KDIR ?= /lib/modules/$(shell uname -r)/build
CPPFLAGS += -Iinclude -Iapp/include
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
APP_SOURCES := app/src/main.cpp app/src/monitor.cpp app/src/driver_client.cpp
APP_BINARY := build/resource-monitor
CPU_TEST_BINARY := build/test-cpu-sampling

.PHONY: all app driver test clean
all: app

app: $(APP_BINARY)

$(APP_BINARY): $(APP_SOURCES) app/include/monitor.hpp app/include/driver_client.hpp include/resource_monitor_abi.h
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(APP_SOURCES) -o $@

driver:
	@if [ ! -f "$(KDIR)/Makefile" ]; then \
		echo "Kernel build tree not found: $(KDIR)"; \
		echo "Install headers matching 'uname -r', then run: make driver KDIR=/path/to/kernel/build"; \
		exit 2; \
	fi
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)/driver" modules

$(CPU_TEST_BINARY): tests/test_cpu_sampling.cpp app/src/monitor.cpp app/src/driver_client.cpp app/include/monitor.hpp app/include/driver_client.hpp include/resource_monitor_abi.h
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_cpu_sampling.cpp app/src/monitor.cpp app/src/driver_client.cpp -o $@

test: app $(CPU_TEST_BINARY)
	@$(CPU_TEST_BINARY)
	@output="$$($(APP_BINARY) --once)" || { echo "Application smoke test failed"; exit 1; }; \
	printf '%s\n' "$$output" | grep -q '^CPU usage:' || { echo "CPU data missing"; exit 1; }; \
	printf '%s\n' "$$output" | grep -q '^RAM usage:' || { echo "RAM data missing"; exit 1; }; \
	printf '%s\n' "$$output" | grep -q '^Root disk:' || { echo "Disk data missing"; exit 1; }; \
	printf '%s\n' "$$output" | grep -q '^Processes:' || { echo "Process data missing"; exit 1; }; \
	printf '%s\n' "$$output" | grep -q '^Sample processes (' || { echo "Process sample missing"; exit 1; }; \
	printf '%s\n' "$$output" | grep -q 'PID.*CPU%.*NAME' || { echo "Process table fields missing"; exit 1; }; \
	echo "Application smoke test passed"

clean:
	rm -rf build
	@if [ -f "$(KDIR)/Makefile" ]; then $(MAKE) -C "$(KDIR)" M="$(CURDIR)/driver" clean; fi
