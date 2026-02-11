# OpenGL Obstacle Runner (2D & 3D)

A 2D and 3D obstacle avoidance game developed in **C++ using OpenGL (GLUT)**.

This project demonstrates real-time rendering, collision detection, state management, procedural obstacle generation, power-up systems, and dynamic difficulty scaling — all built without using a game engine.

---

## 📌 Project Overview

This repository contains **two implementations** of the same game concept:

### 🟦 2D Version
A top-down obstacle avoidance game where the player moves left and right to avoid falling obstacles.

### 🟥 3D Version
A perspective-based endless runner featuring depth simulation, forward movement illusion, and enhanced visual rendering.

Both versions were built from scratch using **OpenGL and GLUT**, without any external game engine.

---

## 🎮 Core Gameplay Mechanics

- Left / Right player movement
- Real-time obstacle spawning
- Collision detection system
- Score tracking
- Time tracking
- Dynamic speed increase over time
- Game states (Menu, Character Select, Playing, Game Over)

---

## 🟪 3D Version Features

- Perspective rendering
- Simulated forward depth movement
- Lane-based obstacle system
- Power-up mechanics:
  - Invincibility
  - Half speed
  - Double score
- Dynamic difficulty scaling

---

## 🧱 Technical Concepts Demonstrated

- OpenGL rendering pipeline
- GLUT window and input handling
- State management using enums
- Procedural gameplay logic
- Frame-based animation updates
- Basic collision detection algorithms
- Real-time game loop structure

---

## 🛠️ Technologies Used

- C++
- OpenGL
- GLUT (OpenGL Utility Toolkit)
- Code::Blocks IDE

---

## 📂 Project Structure

```
OpenGL-Obstacle-Runner-2D-3D/
│
├── 2D-version/
│   └── Project2D.cbp
│   └── main.cpp
│
├── 3D-version/
│   └── Project3D.cbp
│   └── main.cpp
│
├── README.md
├── LICENSE
└── .gitignore
```

---

## ▶️ How to Run

1. Open the project in **Code::Blocks**
2. Ensure OpenGL and GLUT libraries are properly configured
3. Build and run either:
   - `2D-version/Project2D.cbp`
   - `3D-version/Project3D.cbp`

---

## 🚀 What This Project Shows

This project highlights:

- Understanding of graphics programming fundamentals
- Ability to implement game mechanics from scratch
- Clean state management design
- Structured C++ programming
- Rendering without a game engine

---

## 🧠 Engineering Decisions

- Designed a structured game state system using enums to manage transitions between Menu, Character Select, Gameplay, and Game Over.
- Implemented procedural obstacle spawning to simulate infinite gameplay.
- Used frame-based updates to ensure consistent animation timing.
- Built collision detection manually without external physics libraries.
- Structured the code for separation between rendering logic and gameplay logic.

---

## 👤 Author

**Mahmoud Hany Amarah**  
Computer Engineering Student  
Focus Areas: Embedded Systems, Networking, Graphics Programming, and Systems Development

---

## 📜 License

This project is licensed under the MIT License.
