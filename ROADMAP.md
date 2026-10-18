# Taz: Wanted Modern Port - Roadmap

## Phase 1: Target Discovery & Entry Point
- [ ] Parse `TAZ.EXE` PE headers and import tables
- [ ] Locate `WinMain` entry point and main window initialization
- [ ] Document DirectX 8 & DirectSound API usages

## Phase 2: Asset Parsing & BlitzTech Engine
- [ ] Unpack and parse BlitzTech `.pc` asset archives from `bin/`
- [ ] Write standalone C++ parser for mesh, texture, and audio structures
- [ ] Extract Taz character model and test rendering

## Phase 3: Engine Architecture & Vulkan/SDL2 Pipeline
- [ ] Configure CMake build system for Debian/Mint C++ target
- [ ] Replace Win32 window creation with SDL2 loop
- [ ] Implement Vulkan rendering backend and basic shaders

## Phase 4: Subsystem Reconstruction
- [ ] Decompile player movement state machine
- [ ] Wire audio playback routines to SDL_mixer or OpenAL
- [ ] Rebind modern gamepad controls via SDL2 Input
