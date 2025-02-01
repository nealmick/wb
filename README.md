# A Very Basic Web Browser

![Screenshot 2025-02-01 at 3 41 24 PM](https://github.com/user-attachments/assets/a3ba2859-9a63-43ae-91d4-c17bfeedab13)

#### Build from source
#### Prerequisites
CMake (version 3.10 or higher)
C++17 compatible compiler
OpenGL
GLFW3
GLEW

Clone the repository with its submodules:
```sh
git clone --recursive https://github.com/nealmick/wb
cd wb
git submodule init
git submodule update

```

Building the Project
```sh
mkdir build
cd build
cmake ..

make

./SimpleBrowser
```

Contributions are welcome.
