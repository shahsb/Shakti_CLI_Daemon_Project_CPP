# Shakti_CLI_Daemon_Project_CPP -- C++ IPC based a daemon and a CLI project:

# [1] STEPS TO SETUP RHEL OS ON WINDOWS USING DOCKER:
---
+ Mentioning this step explicitly just in case if you don't have RHEL VM.
+ You could setup RHEL UBI 9 Docker image on Windows Machine using docker:
#### STEPS:
  1) Install docker on windows
  2) `docker pull registry.access.redhat.com/ubi9/ubi`
  3) `docker run -it --name rhel-ubi9 registry.access.redhat.com/ubi9/ubi /bin/bash`
  
#### STEPS TO SETUP C++ DEV TOOLS:
  1) Install compiler + build tools
     ```
     dnf groupinstall -y "Development Tools"
     ```

  3) Useful extras
     ```
     dnf install -y cmake git vim-enhanced diffutils valgrind
     dnf install -y gcc-c++
     ```
		
# [2] BUILD INSTRUCTIONS:
---
#### DEPENDENCIES:
  + GCC compiler (g++) with C++17 support
  + Linux/Unix-like operating system
  + pthread library (included with standard C++ distribution)

#### BUILD STEPS:
1) SAVE ALL FILES IN THE SAME DIRECTORY:
    + Makefile
    + common.h
    + daemon.cpp
    + cli.cpp

2) BUILD THE PROJECT:
   ```
   make
   ```

3) RUN THE DAEMON (IN ONE TERMINAL):
   ```
   ./number_daemon
   ```

4) RUN THE CLI (IN ANOTHER TERMINAL) -- NOTE: Multiple CLIs from Multiple terminals can be opened at once:
   ```
   ./number_cli
   ```

5) TO CLEAN UP:
   ```
   make clean
   ```

# [3] DATA STRUCTURE REASONING:
---
I chose std::map<int32_t, int64_t> for the number store with the following justification:

#### Why std::map?
1) **Automatic Sorting:** std::map maintains elements in sorted order by key (number), making PRINT_ALL operations efficient without additional sorting.
2) **Duplicate Prevention:** Keys in std::map are unique, automatically preventing duplicate entries.
3) **Efficient Operations:**
    + Insertion: O(log n) - Efficient for the expected use case
    + Deletion: O(log n) - Efficient single element removal
    + Search/Lookup: O(log n) - Fast existence checking for FIND operations
    + Sorted Iteration: O(n) - Efficient for PRINT_ALL
4) **Memory Efficiency:** Stores only unique numbers with their timestamps.
5) **Concurrency Support:** Works well with read-write locks (shared_mutex) allowing multiple concurrent reads.

# [4] ALTERNATIVES CONSIDERED:
---
+ **std::set:** Would require storing pairs, less intuitive for key-value storage
+ **std::unordered_map:** Faster O(1) operations but no automatic sorting, requiring extra step for PRINT_ALL
+ **std::vector:** Would require manual sorting and duplicate checking, less efficient
+ **The std::map** provides the best balance of required operations while naturally satisfying all constraints: automatic sorting, duplicate prevention, and efficient operations for the expected workload.
