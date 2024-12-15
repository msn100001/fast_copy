# Fast Copy

Fast Copy is a high-performance, multi-threaded file and directory copying program designed for Linux and Windows Subsystem for Linux (WSL). It dynamically selects the most efficient copying method based on file size, supports logging to a dedicated system directory, and ensures resilience for large-scale operations.

---

## Features

- **Dynamic Copying Methods:**
  - `sendfile` for large files (≥ 100 MB).
  - `mmap` for medium-sized files.
  - Basic Asynchronous I/O (AIO) for small files.
- **Multi-threading:** Concurrent file copying using a thread pool (default: 8 threads).
- **Recursive Directory Traversal:** Copies entire directory trees, preserving the structure.
- **Robust Logging:** Logs execution details (successes, errors, total file count) to `/var/log/fast-copy/`. Log files are timestamped (e.g., `fast-copy-YYYYMMDD-HHMMSS.log`).
- **File Count Display:** Outputs the total number of successfully copied files.
- **Error Resilience:** Graceful handling of errors with detailed logging.

---

## System Requirements

- **Operating System:** Linux or Windows Subsystem for Linux (WSL).
- **Dependencies:**
  - **GCC Compiler:** Required to build the program.
  - **POSIX Libraries:** Supports threading and system calls (`pthread`, `sys/sendfile`, `aio`, etc.).

---

## Installation

### Clone the Repository

```bash
git clone https://github.com/your-username/fast-copy.git
cd fast-copy
```

### Compile the Program

Run the following command to compile the program:

```bash
gcc -g fast_copy.c -o fast_copy -pthread -lrt
```

---

## Usage

### Basic Usage

Run the program with a source and destination directory:

```bash
sudo ./fast_copy /path/to/source_directory /path/to/destination_directory
```

- **Source Directory:** Directory containing files to copy.
- **Destination Directory:** Directory where files will be copied.

### Example

To copy files from `/mnt/source` to `/mnt/destination`:

```bash
sudo ./fast_copy /mnt/source /mnt/destination
```

---

## Log Files

Execution details are saved in `/var/log/fast-copy/`. For example:

```bash
/var/log/fast-copy/fast-copy-20231215-150000.log
```

View logs:

```bash
cat /var/log/fast-copy/fast-copy-20231215-150000.log
```

---

## Program Output

The program will:

- Display progress messages on the console.
- Output the total file count upon completion.

### Sample Output:

```text
Starting copy process: /mnt/source -> /mnt/destination
INFO: File copied using sendfile: /mnt/source/large-file.bin -> /mnt/destination/large-file.bin
INFO: File copied using mmap: /mnt/source/medium-file.txt -> /mnt/destination/medium-file.txt
INFO: File copied using AIO: /mnt/source/small-file.txt -> /mnt/destination/small-file.txt
Total files copied: 42
Copy process completed.
```

---

## How It Works

### Dynamic Copy Methods:

Files are copied using the best available method based on their size:

- **Large files:** `sendfile()` system call for efficient kernel-level copying.
- **Medium files:** `mmap()` for memory-mapped file copying.
- **Small files:** Asynchronous I/O (basic implementation).

### Multithreading:

Uses a thread pool with a maximum of 8 threads (configurable). Files are copied concurrently to maximize throughput.

### Logging:

Dedicated log files are created in `/var/log/fast-copy/`. Logs include:

- Start and end of the copy process.
- Errors encountered during execution.
- Details of successfully copied files.

---

## Error Handling

If a file or directory fails to copy, the program logs the error and continues processing other files. Files are not overwritten unless explicitly required.

---

## Known Issues

- The program requires `sudo` permissions to write logs to `/var/log/fast-copy/`.
- Large directories with thousands of files may take longer due to recursion overhead.

---

## Future Improvements

- Add support for incremental file copying.
- Implement better AIO with fully asynchronous reads and writes.
- Optimize for disk I/O throttling on high-load systems.

---

## Contributing

1. Fork the repository.
2. Create a new branch for your feature:

    ```bash
    git checkout -b feature-name
    ```

3. Commit your changes and push:

    ```bash
    git commit -m "Add new feature"
    git push origin feature-name
    ```

4. Create a pull request.

---

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

---

## Acknowledgments

Developed for fast and efficient file copying in Linux environments. Inspired by the need for robust tools that handle large-scale file transfers efficiently.

---

## Contact

- **Name:** MSN
- **Email:** [mattnel33@gmail.com](mailto:mattnel33@gmail.com)

