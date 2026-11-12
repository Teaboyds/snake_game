# Snake (C++17) — โครงสร้างแบบ game engine

เกมงูบน terminal (macOS / Linux) ที่วางโครงแบบเดียวกับเกมใหญ่ ๆ เพื่อใช้ศึกษา game backend

📘 **เอกสาร**
- [docs/RUNNING.md](docs/RUNNING.md) — ติดตั้ง, build, เล่น, ปรับค่า, แก้ปัญหา
- [docs/LEARNING.md](docs/LEARNING.md) — อธิบายสถาปัตยกรรมทีละบท + แบบฝึกหัด

## Build & Run

```bash
cmake -S . -B build
cmake --build build -j
./build/snake          # เล่นเกม
./build/snake_tests    # รัน unit tests
```

หรือใช้ทางลัด (configure + build + รันในคำสั่งเดียว):

```bash
./snake run start      # เล่นเกม
./snake run test       # รัน unit tests
```

อยากพิมพ์แค่ `snake run start` จากที่ไหนก็ได้ ให้ลิงก์สคริปต์เข้า PATH ครั้งเดียว:

```bash
ln -s "$PWD/snake" ~/.local/bin/snake
```

ปุ่ม: ลูกศร / WASD เดิน · P หรือ Esc หยุด · Enter เริ่ม · Q ออก

คะแนนเก็บใน Redis (`127.0.0.1:6380` ค่าเริ่มต้น, เปลี่ยนด้วย `SNAKE_REDIS_HOST` / `SNAKE_REDIS_PORT`) — ต่อไม่ได้ก็เล่นแบบ offline ได้

## โครงสร้าง

```
src/
├── engine/                 ← ไม่รู้จัก "งู" เลย ใช้ซ้ำกับเกมอื่นได้
│   ├── core/Application    game loop (fixed timestep)
│   ├── platform/Terminal   ชั้นเดียวที่คุยกับ OS (termios, ANSI)
│   ├── input/InputSystem   raw bytes → Action (Up/Down/Confirm/...)
│   ├── render/             FrameBuffer (back buffer) + TerminalRenderer (delta render)
│   └── scene/              Scene interface + SceneManager (state machine)
├── game/
│   ├── GameConfig.h        ค่า tuning ทั้งหมด (data-driven)
│   ├── Session.h           ข้อมูลข้าม scene (high score)
│   ├── sim/                ★ simulation ล้วน: Snake, World — ไม่มี I/O
│   └── scenes/             Menu / Play / GameOver
└── main.cpp
tests/TestMain.cpp          ทดสอบ simulation โดยไม่ต้องมีจอ
```

ทิศทางการพึ่งพา: `platform ← engine ← game ← main` (ชั้นล่างไม่รู้จักชั้นบน)

## การไหลของข้อมูลในหนึ่งเฟรม

```
Keyboard ─► InputSystem ─► Action ─► PlayScene ─► TurnCommand ─► World.step()
                                                                    │
FrameBuffer ◄── PlayScene.render() ◄── state ◄──────────────────────┤
     │                                                              ▼
TerminalRenderer (ส่งเฉพาะ cell ที่เปลี่ยน)          GameEvent (FruitEaten, SnakeDied...)
```

## แนวคิดที่ควรศึกษาต่อ (และเกี่ยวกับ backend โดยตรง)

| แนวคิด | อยู่ที่ไหน | ทำไมสำคัญกับ game backend |
|---|---|---|
| Fixed timestep | `Application::run` | simulation ให้ผลเหมือนกันทุกเครื่อง ไม่ขึ้นกับ FPS |
| Simulation แยกจาก presentation | `game/sim/` | ย้าย `World` ไปรันบน server ได้ทั้งก้อน (authoritative server) |
| Command in / Event out | `TurnCommand`, `GameEvent` | client ส่ง command ไม่ใช่ state; event ใช้ทำ UI, เสียง, analytics, network |
| Determinism (seed) | `World(config, seed)` | ทำ replay, anti-cheat (server รันซ้ำตรวจผล), lockstep multiplayer |
| Delta rendering | `TerminalRenderer` | หลักเดียวกับ delta snapshot ที่ server ส่งให้ client |
| Deferred state change | `SceneManager` | ไม่แก้โครงสร้างข้อมูลขณะกำลังวนลูปมันอยู่ |
| RAII | `Terminal` | คืนทรัพยากรเสมอแม้เกิด exception |

## แบบฝึกหัดต่อยอด

1. **Replay**: บันทึก `(seed, [(tick, TurnCommand)...])` ลงไฟล์ แล้วเล่นซ้ำให้ได้ผลเหมือนเดิมเป๊ะ
2. **Headless server**: เขียน `server_main.cpp` ที่รัน `World` อย่างเดียวผ่าน TCP/UDP, ให้ `snake` เป็น client
3. **Multiplayer**: งูหลายตัวใน `World` เดียว + ชนกันเอง
4. **Config จากไฟล์**: โหลด `GameConfig` จาก JSON
5. **ECS**: ลองแยก Snake/Fruit เป็น entity + component แล้วเทียบข้อดีข้อเสียกับแบบปัจจุบัน
