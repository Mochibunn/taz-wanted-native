# TAZ Decomp

This project is for decompiling and analyzing the TAZ (Turtle Beach) audio driver for the PS3. The goal is to understand how audio is handled in the PS3's console environment and potentially create a compatible open-source replacement.

## Getting Started

These instructions will get you a copy of the project up and running on your local machine.

### Prerequisites

You'll need:
- A Linux system (Ubuntu 20.04 LTS or newer recommended)
- Development tools: build-essential, cmake, git
- PS3 SDK (or similar PS3 development environment)

### Cloning the Repository

To clone this repository, run:

```
git clone https://github.com/yourusername/taz-decomp.git
cd taz-decomp
```

### Building the Project

To build the project, run:

```
mkdir build && cd build
cmake ..
make
```

## Project Structure

- `src/` - Source code files
- `include/` - Header files
- `lib/` - Libraries needed for compilation
- `docs/` - Documentation for the project.

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of conduct, and the process for submitting pull requests to us.

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details.