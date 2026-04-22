# Custom Memory Allocator

A **high-performance, thread-safe memory management system** implemented in C. This project features a custom heap implementation with dual allocation strategies, a REST API for remote management, and a real-time web-based visualization dashboard.

## 🎯 Features

  * **Core Allocator:** Mutex-based thread safety, linear scan coalescing to prevent fragmentation, and heap integrity verification.
  * **Dual Strategies:** Toggle between **Best-Fit** (minimizes fragmentation) and **First-Fit** (reduces allocation latency).
  * **REST API:** Fully featured server on port `8081` with endpoints for `/api/malloc`, `/api/free`, and `/api/status`.
  * **Web GUI:** Interactive dashboard to visualize the heap memory map, monitor statistics, and inspect blocks in real-time.

-----

## 🚀 Quick Start (5-Minute Setup)

### 1\. Prerequisites

  * **Compiler:** GCC (C11 support)
  * **Tools:** GNU Make, Pthread library
  * **Environment:** Linux, macOS, or WSL

### 2\. Build and Run

```bash
# Clone the repository
git clone https://github.com/yourusername/custom-allocator.git
cd custom-allocator

# Build the release version (Best-Fit default)
make release

# Start the server
make run
```

### 3\. Access the Dashboard

Open your browser to: **`http://localhost:8081`**

-----

## 🛠 Professional Insights & Learning Outcomes

Developed with a focus on **Systems Programming** and **Firmware Engineering**, this project demonstrates mastery of low-level concepts:

  * **Memory Safety:** Manual pointer arithmetic and header-to-payload offsets, crucial for bare-metal development.
  * **RTOS Concepts:** Using `pthread_mutex` to manage shared resources, simulating a multi-threaded Real-Time Operating System environment.
  * **Algorithm Trade-offs:** Implementing **Best-Fit** vs. **First-Fit** to analyze the balance between fragmentation overhead and execution latency.
  * **Firmware Reliability:** The `allocator_verify()` logic mirrors integrity checks used in SSD firmware to detect and prevent data corruption.

-----

## 📋 System Architecture

### Memory Block Metadata

Every block on the heap is preceded by a metadata header that manages the linked-list state:

```c
typedef struct Block {
    size_t size;              // Usable payload size
    bool is_free;             // Availability status
    struct Block *next;       // Pointer to the next block
    time_t allocated_time;    // Timestamp for analysis
    pid_t allocator_thread;   // Owner thread ID
} Block;
```

### Strategy Complexity

| Operation | Best-Fit | First-Fit |
| :--- | :--- | :--- |
| **Allocation** | **O(n)** (Full scan) | **O(k)** (Early exit) |
| **Free/Coalesce** | **O(1)** | **O(1)** |
| **Fragmentation** | Low | Moderate |

-----

## 🧪 Testing & Quality

  * **Unit Tests:** Run `make test` to execute the validation suite.
  * **Memory Leaks:** Check for leaks using Valgrind: `make memcheck`.
  * **Static Analysis:** Ensure code quality: `make analyze`.

-----

## 📂 Project Structure

```text
.
├── docs/                   # API and Architecture guides
├── include/                # Public header files (allocator.h)
├── src/                    # Core logic (main.c, best_fit.c, utils.c)
├── tests/                  # Validation suite
├── web/                    # Dashboard GUI (HTML, CSS, JS)
├── LICENSE                 # MIT License
├── Makefile                # Unified build system
└── README.md               # Project Hub
```

-----

## 🛠 Advanced Setup

### Running as a Linux Service (Systemd)

To run the allocator as a persistent background daemon:

1.  **Install Binary:** `sudo cp build/bin/allocator_server /usr/local/bin/`
2.  **Create Service:** Create `/etc/systemd/system/allocator.service`:
    ```ini
    [Unit]
    Description=Custom Memory Allocator Server
    After=network.target

    [Service]
    ExecStart=/usr/local/bin/allocator_server
    Restart=on-failure

    [Install]
    WantedBy=multi-user.target
    ```
3.  **Enable:** `sudo systemctl enable --now allocator`

### 🔍 Troubleshooting

**Port 8081 already in use?**

```bash
lsof -i :8081
kill -9 <PID>
```

-----

## 📄 License

Distributed under the **MIT License**. See `LICENSE` for more information.

**Built for the love of low-level systems.** 🚀

-----

### Final Instructions for You:

1.  **Paste** this into your `README.md`.
2.  **Delete** the now-unnecessary files:
    ```bash
    rm QUICKSTART.md PROJECT_SUMMARY.md INDEX.md docs/DEPLOYMENT.md
    ```
3.  **Run** `make clean` before your final git push to keep the repo size small.
