# Basic Language Project

This project implements a simple programming language using C++ and LLVM. The language supports basic arithmetic operations, function definitions, and variable assignments. The project is organized into multiple files for better modularity and maintainability.

## Project Structure

```
basic-lang
├── src
│   ├── ast.cpp          # Implementation of the Abstract Syntax Tree (AST) classes
│   ├── ast.h            # Declarations of the AST classes
│   ├── codegen.cpp      # Code generation logic for the AST classes
│   ├── codegen.h        # Declarations for code generation functions and classes
│   ├── lexer.cpp        # Lexer implementation for tokenizing input source code
│   ├── lexer.h          # Declarations for the lexer class
│   ├── parser.cpp       # Parser implementation for constructing the AST
│   ├── parser.h         # Declarations for the parser class
│   └── main.cpp         # Entry point for the application
├── CMakeLists.txt       # CMake configuration file
└── README.md            # Project documentation
```

## Building the Project

To build the project, you need to have CMake and LLVM installed on your system. Follow these steps:

1. Clone the repository or download the project files.
2. Open a terminal and navigate to the project directory.
3. Create a build directory:
   ```
   mkdir build
   cd build
   ```
4. Run CMake to configure the project:
   ```
   cmake ..
   ```
5. Build the project:
   ```
   make
   ```

## Running the Application

After building the project, you can run the application using the following command:

```
./basic-lang
```

You will see a prompt where you can enter expressions, function definitions, and variable assignments. The interpreter will evaluate the input and display the results.

## Contributing

Contributions to the project are welcome! If you have suggestions or improvements, feel free to submit a pull request or open an issue.

## License

This project is licensed under the MIT License. See the LICENSE file for more details.