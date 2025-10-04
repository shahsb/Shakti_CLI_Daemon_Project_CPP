#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <ctime>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <limits>

#include "common.h"

class NumberCLI {
private:
    std::string socket_path;
    
public:
    NumberCLI(const std::string& path) : socket_path(path) {}
    
    int connect_to_daemon() {
        int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Error: Failed to create socket" << std::endl;
            return -1;
        }
        
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
        
        if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Error: Cannot connect to daemon. Is the daemon running?" << std::endl;
            close(sockfd);
            return -1;
        }
        
        return sockfd;
    }
    
    void show_menu() {
        std::cout << "\n=== Number Store CLI ===" << std::endl;
        std::cout << "1. Insert a number" << std::endl;
        std::cout << "2. Delete a number" << std::endl;
        std::cout << "3. Print all numbers" << std::endl;
        std::cout << "4. Delete all numbers" << std::endl;
        std::cout << "5. Find a number" << std::endl;
        std::cout << "6. Exit" << std::endl;
        std::cout << "Choose an option (1-6): ";
    }
    
    int get_positive_integer(const std::string& prompt) {
        int number;
        while (true) {
            std::cout << prompt;
            std::cin >> number;
            
            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Error: Invalid input. Please enter a positive integer." << std::endl;
            } else if (number <= 0) {
                std::cout << "Error: Please enter a positive integer." << std::endl;
            } else {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                return number;
            }
        }
    }
    
    void insert_number() {
        int number = get_positive_integer("Enter number to insert: ");
        
        int sockfd = connect_to_daemon();
        if (sockfd < 0) return;
        
        IPCMessage msg;
        msg.type = MessageType::INSERT;
        msg.number = number;
        
        if (write(sockfd, &msg, sizeof(msg)) < 0) {
            std::cerr << "Error: Failed to send message to daemon" << std::endl;
            close(sockfd);
            return;
        }
        
        IPCMessage response;
        if (read(sockfd, &response, sizeof(response)) > 0) {
            if (response.type == MessageType::RESPONSE_SUCCESS) {
                std::cout << "Number " << response.number << " inserted successfully." << std::endl;
                std::cout << "Timestamp: " << format_timestamp(response.timestamp) << std::endl;
            } else {
                std::cout << response.error_msg << std::endl;
            }
        }
        
        close(sockfd);
    }
    
    void delete_number() {
        int number = get_positive_integer("Enter number to delete: ");
        
        int sockfd = connect_to_daemon();
        if (sockfd < 0) return;
        
        IPCMessage msg;
        msg.type = MessageType::DELETE;
        msg.number = number;
        
        if (write(sockfd, &msg, sizeof(msg)) < 0) {
            std::cerr << "Error: Failed to send message to daemon" << std::endl;
            close(sockfd);
            return;
        }
        
        IPCMessage response;
        if (read(sockfd, &response, sizeof(response)) > 0) {
            if (response.type == MessageType::RESPONSE_SUCCESS) {
                std::cout << "Number " << response.number << " deleted successfully." << std::endl;
            } else {
                std::cout << response.error_msg << std::endl;
            }
        }
        
        close(sockfd);
    }
    
    void print_all_numbers() {
        int sockfd = connect_to_daemon();
        if (sockfd < 0) return;
        
        IPCMessage msg;
        msg.type = MessageType::PRINT_ALL;
        
        if (write(sockfd, &msg, sizeof(msg)) < 0) {
            std::cerr << "Error: Failed to send message to daemon" << std::endl;
            close(sockfd);
            return;
        }
        
        std::vector<NumberEntry> entries;
        IPCMessage response;
        
        // Read initial response
        if (read(sockfd, &response, sizeof(response)) > 0) {
            if (response.type == MessageType::RESPONSE_SUCCESS) {
                // Read data messages until end marker
                while (read(sockfd, &response, sizeof(response)) > 0) {
                    if (response.type == MessageType::RESPONSE_DATA) {
                        entries.emplace_back(response.number, response.timestamp);
                    } else if (response.type == MessageType::RESPONSE_SUCCESS && response.number == -1) {
                        break; // End marker
                    }
                }
            } else {
                std::cout << response.error_msg << std::endl;
                close(sockfd);
                return;
            }
        }
        
        close(sockfd);
        
        if (entries.empty()) {
            std::cout << "No numbers stored." << std::endl;
        } else {
            std::cout << "\nStored numbers (sorted):" << std::endl;
            std::cout << std::setw(10) << "Number" << " | " << "Timestamp" << std::endl;
            std::cout << std::string(35, '-') << std::endl;
            
            for (const auto& entry : entries) {
                std::cout << std::setw(10) << entry.number << " | " 
                         << format_timestamp(entry.timestamp) << std::endl;
            }
        }
    }
    
    void delete_all_numbers() {
        int sockfd = connect_to_daemon();
        if (sockfd < 0) return;
        
        IPCMessage msg;
        msg.type = MessageType::DELETE_ALL;
        
        if (write(sockfd, &msg, sizeof(msg)) < 0) {
            std::cerr << "Error: Failed to send message to daemon" << std::endl;
            close(sockfd);
            return;
        }
        
        IPCMessage response;
        if (read(sockfd, &response, sizeof(response)) > 0) {
            if (response.type == MessageType::RESPONSE_SUCCESS) {
                std::cout << "All numbers deleted successfully." << std::endl;
            } else {
                std::cout << response.error_msg << std::endl;
            }
        }
        
        close(sockfd);
    }
    
    void find_number() {
        int number = get_positive_integer("Enter number to find: ");
        
        int sockfd = connect_to_daemon();
        if (sockfd < 0) return;
        
        IPCMessage msg;
        msg.type = MessageType::FIND;
        msg.number = number;
        
        if (write(sockfd, &msg, sizeof(msg)) < 0) {
            std::cerr << "Error: Failed to send message to daemon" << std::endl;
            close(sockfd);
            return;
        }
        
        IPCMessage response;
        if (read(sockfd, &response, sizeof(response)) > 0) {
            if (response.type == MessageType::RESPONSE_SUCCESS) {
                if (response.timestamp != -1) {
                    std::cout << "Number " << response.number << " found." << std::endl;
                    std::cout << "Inserted at: " << format_timestamp(response.timestamp) << std::endl;
                } else {
                    std::cout << "Number " << response.number << " not found." << std::endl;
                }
            } else {
                std::cout << response.error_msg << std::endl;
            }
        }
        
        close(sockfd);
    }
    
    std::string format_timestamp(int64_t timestamp) {
        std::time_t time = timestamp;
        std::tm* tm_info = std::localtime(&time);
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        return std::string(buffer) + " (" + std::to_string(timestamp) + ")";
    }
    
    void run() {
        std::cout << "Number Store CLI - Connected to daemon" << std::endl;
        
        while (true) {
            show_menu();
            
            int choice;
            std::cin >> choice;
            
            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Error: Invalid input. Please enter a number between 1-6." << std::endl;
                continue;
            }
            
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            
            switch (choice) {
                case 1:
                    insert_number();
                    break;
                case 2:
                    delete_number();
                    break;
                case 3:
                    print_all_numbers();
                    break;
                case 4:
                    delete_all_numbers();
                    break;
                case 5:
                    find_number();
                    break;
                case 6:
                    std::cout << "Goodbye!" << std::endl;
                    return;
                default:
                    std::cout << "Error: Invalid choice. Please enter a number between 1-6." << std::endl;
                    break;
            }
        }
    }
};

int main() {
    NumberCLI cli("/tmp/number_daemon.sock");
    cli.run();
    return 0;
}