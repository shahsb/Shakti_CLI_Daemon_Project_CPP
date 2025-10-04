CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread
TARGET_DAEMON = number_daemon
TARGET_CLI = number_cli
SOCKET_PATH = /tmp/number_daemon.sock

.PHONY: all clean run-daemon

all: $(TARGET_DAEMON) $(TARGET_CLI)

$(TARGET_DAEMON): daemon.cpp
        $(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_CLI): cli.cpp
        $(CXX) $(CXXFLAGS) -o $@ $^

run-daemon: $(TARGET_DAEMON)
        ./$(TARGET_DAEMON)

clean:
        rm -f $(TARGET_DAEMON) $(TARGET_CLI) $(SOCKET_PATH)