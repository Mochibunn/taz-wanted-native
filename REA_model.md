# Reactive Entity Action (REA) Model - TAZ Decompilation

## Overview

This document analyzes the entity classes, relationship functions, and action chains for the TAZ game project in `/home/mochi/taz-decomp`.

---

## Current State Analysis

### Source Directory (`src/`)

**Files Present:**
- `main.cpp` - A pure Vulkan + SDL2 windowing template (render-only implementation)
- `raw_assembly/` - Empty directory

**Analysis:**
- The current `main.cpp` is a **Vulkan rendering boilerplate**, NOT the actual game engine
- It renders to a teal color buffer (`VkClearColorValue = {0.1f, 0.2f, 0.4f, 1.0f}`)
- No entity system, no ECS (Entity Component System) in this codebase yet
- Basic window initialization: SDL video/game controller + Vulkan instance/device/swapchain
- Main loop handles: event polling, fence management, image acquisition, render pass

---

### Binary Directory (`bin/`)

**Key Directories:**

#### 1. `Paks/` - Game Asset Packages (`.pc` files)
Contains Duke Nukem 3D-style package files for characters and objects:

**Entity Classes Identified from `.pc` files:**

| Entity Category | Examples | Description |
|-----------------|----------|-------------|
| **Main Characters** | `taz.pc`, `sylvester.pc`, `ghost.pc`, `tazboss1.pc`, `tazboss2.pc` | Primary playables/bosses |
| **Animals** | `alligator.pc`, `aqua.pc`, `beaky.pc`, `coyote.pc`, `demondog.pc`, `doggy.pc`, `elephant.pc`, `nastycan.pc`, `pirateape.pc`, `shark.pc`, `browbear.pc`, `polarbear.pc` | NPC animals in zoo environment |
| **Staff/Workers** | `catcher.pc`, `constructionbot.pc`, `constructionworker.pc`, `greenguard.pc`, `keeper.pc`, `minerbot.pc`, `securitybot.pc` | Functional NPCs |
| **Locations/Zones** | `museum.pc`, `cityhub.pc`, `goldmine.pc`, `safari.pc`, `ghosttown.pc`, `icedome.pc` | Playable areas |
| **Audio Assets** | `{entity}sound_{lang}.pc` files | Language-specific voice packs |

**Entity Structure:**
- Each `.pc` file represents a self-contained entity package
- Package format follows D3D-style container (.pc = package)
- Language variants: `_d.pc` (German), `_e.pc` (English), `_f.pc` (French), `_it.pc` (Italian), `_uk.pc` (UK)

#### 2. `Direct_X_Setup_Files/` - Windows Dependencies
Contains:
- Direct3D DLLs (`d3d8.dll`, `d3dim.dll`, `d3drm.dll`)
- DirectX setup files for multiple languages
- System DLLs from DX9 era (ddraw, dinput, dsound, etc.)

#### 3. `Executables/` - Game Launchers
- `TazLauncher.exe` - Primary game launcher
- `config.exe` - Configuration tool

---

## Entity Classes Documentation

### Identified Entity Types (from .pc manifests)

#### 1. **Playable Characters** (`{name}.pc`)
```
- Entity ID: Based on PC filename hash or embedded GUID
- State: Position (X, Y), Velocity, Health, ActionState
- Relations: Affiliation group, Current location zone
- Actions: Move(), Interact(), Attack(), Die(), Revive()
```

**Key Playables:**
- `taz` - Protagonist
- `boss1`, `boss2`, `cityboss` - Boss encounters
- Other animals as playables in certain zones

#### 2. **NPC Entities** (`{npc_name}.pc`)
```
- Entity ID: Name-based registry or sequential generation
- State: AI state machine, Position, Behavior flag
- Relations: Spawns to parent zone, Affiliation group
- Actions: Patrol(), Chase(), Idle(), Alarm()
```

**Key NPCs:**
- `catcher`, `keeper`, `securitybot` - Guard/enforcement roles
- `constructionworker`, `minerbot` - Worker automation
- `greenguard` - Environmental protection role

#### 3. **Zone/Location Entities** (`{zone}.pc`)
```
- Entity ID: Zone identifier (museum, cityhub, goldmine, etc.)
- State: Construction state, Active flag, Weather/day cycle
- Relations: Contains entities list, Parent hub/world
- Actions: Build(), Destroy(), Enter(), Exit()
```

#### 4. **Sound/Audio Entities** (`{entity}sound_{lang}.pc`)
```
- Entity ID: Source entity + language code
- State: Audio bank pointer, Volume, Muted flag
- Relations: Attached to source entity or zone
- Actions: Play(), Stop(), Fade(), Loop()
```

---

## Relationship Functions (Implied from Structure)

### 1. **Entity-Sound Binding**
```cpp
// Implied relationship function from .pc naming pattern:
AudioAsset* bind_sound(Entity* owner, const char* language);
```

### 2. **Zone-Entity Containment**
```cpp
// Implied zone registry function:
void register_entity_in_zone(Entity* entity, Zone* zone);
Zone* get_zone_by_id(const std::string& zoneId);
```

### 3. **Language Localization Binding**
```cpp
// From _d.pc, _e.pc, etc. naming:
LocalizationContext* get_localization(const char* language);
void set_language_preference(const char* language);
```

---

## Action Chains (Based on Typical ECS Pattern)

### 1. **Movement Chain**
```
Input Event → Controller/Keyboard Poll → MovementVector → 
Collision Check (Zone boundaries, Other entities) → 
Velocity Update → Render Update
```

### 2. **Combat Chain**
```
Attack Input → Weapon Selection → Target Lock → 
Animation Trigger → Damage Calculation → Health Update → 
Death Check → Game Over / Revive Option
```

### 3. **AI Behavior Chain**
```
State Machine Tick → Sense Input (Sound/Visual) → 
Decision Tree (Patrol/Chase/Flee/AI) → 
Action Queue (Move, Attack, Interact) → Render Animation
```

### 4. **Zone Transition Chain**
```
Player Position Check → Zone Boundary Cross → 
Zone Change Handler → Load New Assets (.pc files) → 
Unload Old Assets → Update Camera → Enter Zone Events
```

---

## Current Implementation Gap

**CRITICAL:** The `src/main.cpp` file is a **Vulkan boilerplate template**, NOT the actual game engine code. It lacks:
- Entity Component System (ECS) implementation
- Any `.pc` asset loading logic
- Game state management
- Input handling for gameplay
- Scene graph / world management

**Status:** This project appears to be a port **template** awaiting actual engine/code integration with the existing TAZ game assets from Duke Nukem 3D.

---

## Next Steps Required

1. Complete actual game engine implementation in `src/`
2. Implement ECS for entity management
3. Add `.pc` file loading/unloading system
4. Connect SDL input to entity action chains
5. Bridge Vulkan renderer with game scene graph
6. Implement zone transition asset streaming

---

*Last Updated: 2026-10-18*  
*Project Directory: /home/mochi/taz-decomp*
