#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "common.h"

class NumberStore {
private:
    std::map<int32_t, int64_t> numbers;  // number -> timestamp mapping
    mutable std::shared_mutex mutex;

public:
    bool insert(int32_t number) {
        std::unique_lock lock(mutex);
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        auto result = numbers.emplace(number, timestamp);
        return result.second; // true if inserted, false if duplicate
    }
    
    bool remove(int32_t number) {
        std::unique_lock lock(mutex);
        return numbers.erase(number) > 0;
    }
    
    void clear() {
        std::unique_lock lock(mutex);
        numbers.clear();
    }
    
    bool contains(int32_t number) const {
        std::shared_lock lock(mutex);
        return numbers.find(number) != numbers.end();
    }
    
    std::vector<NumberEntry> getAllSorted() const {
        std::shared_lock lock(mutex);
        std::vector<NumberEntry> result;
        result.reserve(numbers.size());
        
        for (const auto& [num, ts] : numbers) {
            result.emplace_back(num, ts);
        }
        
        // Numbers are already sorted in the map by key
        return result;
    }
    
    size_t size() const {
        std::shared_lock lock(mutex);
        return numbers.size();
    }
};

class NumberDaemon {
private:
    NumberStore store;
    int server_fd;
    std::string socket_path;
    bool running;
    std::vector<std::thread> client_threads;
    
public:
    NumberDaemon(const std::string& path) : socket_path(path), running(false) {
        setup_signal_handlers();
    }
    
    ~NumberDaemon() {
        stop();
    }
    
    bool start() {
        // Create socket
        server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        
        // Remove existing socket file
        unlink(socket_path.c_str());
        
        // Bind socket
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
        
        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Failed to bind socket" << std::endl;
            close(server_fd);
            return false;
        }
        
        // Listen for connections
        if (listen(server_fd, 10) < 0) {
            std::cerr << "Failed to listen on socket" << std::endl;
            close(server_fd);
            return false;
        }
        
        // Set socket permissions
        chmod(socket_path.c_str(), 0666);
        
        running = true;
        std::cout << "Number daemon started on " << socket_path << std::endl;
        
        return true;
    }
    
    void run() {
        while (running) {
            int client_fd = accept(server_fd, nullptr, nullptr);
            if (client_fd < 0) {
                if (running) {
                    std::cerr << "Failed to accept client connection" << std::endl;
                }
                continue;
            }
            
            // Handle client in separate thread
            client_threads.emplace_back(&NumberDaemon::handle_client, this, client_fd);
        }
        
        // Clean up client threads
        for (auto& thread : client_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    
    void stop() {
        running = false;
        if (server_fd >= 0) {
            close(server_fd);
            server_fd = -1;
        }
        unlink(socket_path.c_str());
    }
    
private:
    void setup_signal_handlers() {
        signal(SIGINT, [](int) {});
        signal(SIGTERM, [](int) {});
    }
    
    void handle_client(int client_fd) {
        IPCMessage msg;
        ssize_t bytes_read;
        
        while ((bytes_read = read(client_fd, &msg, sizeof(msg))) > 0) {
            process_message(client_fd, msg);
        }
        
        close(client_fd);
    }
    
    void process_message(int client_fd, const IPCMessage& msg) {
        IPCMessage response;
        memset(&response, 0, sizeof(response));
        
        switch (msg.type) {
            case MessageType::INSERT: {
                if (msg.number <= 0) {
                    response.type = MessageType::RESPONSE_ERROR;
                    strncpy(response.error_msg, "Error: Only positive integers are allowed", 
                           sizeof(response.error_msg));
                } else if (store.insert(msg.number)) {
                    response.type = MessageType::RESPONSE_SUCCESS;
                    response.number = msg.number;
                    // Get the timestamp for the response
                    auto entries = store.getAllSorted();
                    for (const auto& entry : entries) {
                        if (entry.number == msg.number) {
                            response.timestamp = entry.timestamp;
                            break;
                        }
                    }
                } else {
                    response.type = MessageType::RESPONSE_ERROR;
                    strncpy(response.error_msg, "Error: Duplicate number not allowed", 
                           sizeof(response.error_msg));
                }
                break;
            }
            
            case MessageType::DELETE: {
                if (msg.number <= 0) {
                    response.type = MessageType::RESPONSE_ERROR;
                    strncpy(response.error_msg, "Error: Only positive integers are allowed", 
                           sizeof(response.error_msg));
                } else if (store.remove(msg.number)) {
                    response.type = MessageType::RESPONSE_SUCCESS;
                    response.number = msg.number;
                } else {
                    response.type = MessageType::RESPONSE_ERROR;
                    strncpy(response.error_msg, "Error: Number not found", 
                           sizeof(response.error_msg));
                }
                break;
            }
            
            case MessageType::PRINT_ALL: {
                auto entries = store.getAllSorted();
                // Send success response first
                response.type = MessageType::RESPONSE_SUCCESS;
                write(client_fd, &response, sizeof(response));
                
                // Send each entry
                for (const auto& entry : entries) {
                    IPCMessage data_msg;
                    data_msg.type = MessageType::RESPONSE_DATA;
                    data_msg.number = entry.number;
                    data_msg.timestamp = entry.timestamp;
                    write(client_fd, &data_msg, sizeof(data_msg));
                }
                
                // Send end marker
                IPCMessage end_msg;
                end_msg.type = MessageType::RESPONSE_SUCCESS;
                end_msg.number = -1; // End marker
                write(client_fd, &end_msg, sizeof(end_msg));
                return; // Don't send another response
            }
            
            case MessageType::DELETE_ALL: {
                store.clear();
                response.type = MessageType::RESPONSE_SUCCESS;
                break;
            }
            
            case MessageType::FIND: {
                if (msg.number <= 0) {
                    response.type = MessageType::RESPONSE_ERROR;
                    strncpy(response.error_msg, "Error: Only positive integers are allowed", 
                           sizeof(response.error_msg));
                } else {
                    response.type = MessageType::RESPONSE_SUCCESS;
                    response.number = msg.number;
                    if (store.contains(msg.number)) {
                        // Get timestamp for found number
                        auto entries = store.getAllSorted();
                        for (const auto& entry : entries) {
                            if (entry.number == msg.number) {
                                response.timestamp = entry.timestamp;
                                break;
                            }
                        }
                    } else {
                        response.timestamp = -1; // Not found marker
                    }
                }
                break;
            }
            
            default:
                response.type = MessageType::RESPONSE_ERROR;
                strncpy(response.error_msg, "Error: Unknown message type", 
                       sizeof(response.error_msg));
                break;
        }
        
        write(client_fd, &response, sizeof(response));
    }
};

int main() {
    NumberDaemon daemon("/tmp/number_daemon.sock");
    
    if (!daemon.start()) {
        std::cerr << "Failed to start daemon" << std::endl;
        return 1;
    }
    
    std::cout << "Number Daemon running. Press Ctrl+C to stop." << std::endl;
    
    daemon.run();
    
    return 0;
}