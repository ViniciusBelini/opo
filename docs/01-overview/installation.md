# Installation

This guide will help you install the Opo Programming Language from source.

## Prerequisites

Before building Opo, make sure your system has the following dependencies installed:

- **GCC**: A C compiler (GCC 9+ recommended)
- **Make**: A build automation tool
- **libffi**: The Foreign Function Interface library
- **pkg-config**: A tool for managing library compile and link flags

### Installing Dependencies (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential libffi-dev pkg-config
```

### Installing Dependencies (macOS)
```bash
brew install libffi pkg-config
```

## Building Opo

1.  Clone the repository:
    ```bash
    git clone https://github.com/ViniciusBelini/opo.git
    cd opo
    ```

2.  Run the build command:
    ```bash
    make
    ```

3.  Verify the installation:
    ```bash
    ./opo
    ```

## Adding Opo to your PATH (Optional)

To run Opo from any directory, move the binary to a directory in your system PATH:

```bash
sudo mv opo /usr/local/bin/
```

Now you can run Opo using:
```bash
opo main.opo
```
