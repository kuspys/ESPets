# ESPets: Modular Cube World for Digital Pets

**ESPets** is an open-source ecosystem of stackable digital pets. Each pet lives inside a physical "Brain" module, and they interact by being stacked together, traversing between cubes via ESP-NOW.

## 🚀 The Architecture: Brain + Backpack

- **The Brain:** A mass-produced ESP32-S3 module with a 1.3-inch square IPS LCD (240x240) and a QMI8658 IMU.
- **The Backpack:** A custom open-source "Connector PCB" that enables physical stacking and power distribution via magnetic pogo pins.
- **The Interaction:** Pets "jump" between modules using **ESP-NOW** when stacking is detected on any of the 4 faces (Top, Bottom, Left, Right).

## 🛠️ Tech Stack
- **Firmware:** C++ / Arduino Framework / PlatformIO.
- **UI/Graphics:** **LVGL** (Light and Versatile Graphics Library) for smooth 60FPS animations.
- **Communication:** **ESP-NOW** for peer-to-peer mesh interaction.
- **Art:** Retro Pixel Art (16-bit color, 240x240 resolution).

## 📂 Project Structure
- `firmware/`: ESP32-S3 source code.
- `hardware/`: KiCad PCB designs for the Backpack and Power Cube.
- `docs/`: Technical specifications and research.
- `assets/`: Pixel art and UI components.

## 📜 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
