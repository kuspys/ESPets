# 🗺️ ESPets Project Roadmap

This document outlines the development path for the ESPets ecosystem, from initial prototyping to a verified open-source community release.

## 📅 Timeline & Milestones (Feb - June 2026)

### **February: The Foundation**
*   **Focus:** Core hardware validation and community setup.
*   **Key Deliverable:** "Hello World" on the custom hardware (ESP32-S3 + 1.3" LCD + IMU).
*   **Community:** Launch Discord and make GitHub public.

### **March: The Hardware**
*   **Focus:** The "Backpack" system and power distribution.
*   **Key Deliverable:** Functional "Backpack PCB" manufactured (CNC) and tested with 4-way stacking logic.
*   **Community:** Open hardware design review (Schematics/PCB).

### **April: The Soul (Art & Interaction)**
*   **Focus:** Pixel art pipeline and web tools.
*   **Key Deliverable:** "Art Bible" release and WebSerial browser-based flashing demo.
*   **Community:** Pixel Art Challenge for community contributions.

### **May: The Digital World**
*   **Focus:** Web mirroring and cloud syncing.
*   **Key Deliverable:** Web Dashboard (React/Next.js) syncing real-time pet state via WiFi.
*   **Community:** Alpha testing with users who own compatible hardware.

### **June: The Release**
*   **Focus:** Documentation and final polish.
*   **Key Deliverable:** Final "Open Source Ecosystem" presentation and Thesis Defense.

---

## 🛠️ Detailed Development Tracks

### 🧱 Phase 1: Prototyping (Hardware & Firmware)
- [ ] **Display:** Validate 1.3" ST7789 IPS LCD with LVGL (60FPS target).
- [ ] **Sensors:** Implement QMI8658 IMU reading (Gyro/Accel) for orientation.
- [ ] **PCB Design:** Design the **"Backpack PCB"** (Connector Module).
    - [ ] Schematic (ESP32 Header -> 4x Pogo Interfaces).
    - [ ] Manufacture prototype (CNC Mill / Etch).
- [ ] **Power:** Build the **"Power Cube"** prototype (USB-C Breakout).
- [ ] **Mesh:** Implement neighbor detection logic (Top/Bottom/Left/Right).
- [ ] **Comms:** "Hello World" ESP-NOW peer-to-peer test.

### 🎨 Phase 2: Art & Ecosystem
- [ ] **Art:** Create the **"Art Bible"** (Color Palette, Pixel Density, Style Guide).
- [ ] **Animations:** Design 3 starter pets (Idle + Walk) in Aseprite.
- [ ] **Pipeline:** Create workflow for converting Aseprite exports to LVGL C-arrays.
- [ ] **WebSerial:** Implement browser-based firmware flashing tool.
- [ ] **Cloud:** Setup Firebase/Supabase for "Live Pet State" syncing.

### 📦 Phase 3: Product Design & Visualization
- [ ] **Enclosure:** Modify STEP files for the new "Brain + Backpack" stack.
- [ ] **Power Cube:** Design and print the dedicated power base enclosure.
- [ ] **Visual Showcase:** Produce a **Launch Trailer** combining **3D Renders** (Exploded View of the assembly) and **Live Shots** of the prototype in action.

## 🤝 How to Contribute
We are currently in the **Prototyping Phase**. The best way to help right now is:
1.  **Spread the word:** Join our [Discord](https://discord.gg/placeholder) (Coming Soon).
2.  **Code:** Review the `firmware/` folder (once populated).
3.  **Art:** Wait for the "Art Bible" release in April to submit pet designs!
