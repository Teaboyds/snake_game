# เรียนรู้จากโปรเจกต์ Snake: สถาปัตยกรรมเกมสำหรับสาย Game Backend

เอกสารนี้พาอ่านโค้ดทีละชั้น อธิบายว่า **ทำไม** ถึงเขียนแบบนี้ และเชื่อมแต่ละแนวคิดเข้ากับงาน game backend จริง

> วิธีอ่านที่แนะนำ: เปิดไฟล์ที่อ้างถึงคู่กันไปด้วย อ่านจบแต่ละบทแล้วลองทำ "ลองเอง" ท้ายบท

---

## สารบัญ

0. [ภาพรวม และลำดับการอ่านโค้ด](#0-ภาพรวม-และลำดับการอ่านโค้ด)
1. [แบ่งชั้น (Layered Architecture)](#1-แบ่งชั้น-layered-architecture)
2. [Game Loop และ Fixed Timestep](#2-game-loop-และ-fixed-timestep)
3. [Platform Layer และ RAII](#3-platform-layer-และ-raii)
4. [Input: จาก byte สู่ Action](#4-input-จาก-byte-สู่-action)
5. [Rendering: FrameBuffer และ Delta Rendering](#5-rendering-framebuffer-และ-delta-rendering)
6. [Scene และ State Machine](#6-scene-และ-state-machine)
7. [Simulation: หัวใจของเกม](#7-simulation-หัวใจของเกม)
8. [ไล่ตามการกดปุ่ม 1 ครั้ง ตั้งแต่ต้นจนจบ](#8-ไล่ตามการกดปุ่ม-1-ครั้ง-ตั้งแต่ต้นจนจบ)
9. [Case Study: การเพิ่ม Wall Event](#9-case-study-การเพิ่ม-wall-event)
10. [Testing](#10-testing)
11. [จากเกมนี้ ไปสู่ Game Server](#11-จากเกมนี้-ไปสู่-game-server)
12. [Leaderboard ด้วย Redis](#12-leaderboard-ด้วย-redis)
13. [แบบฝึกหัด](#13-แบบฝึกหัด)
14. [คำศัพท์](#14-คำศัพท์)

---

## 0. ภาพรวม และลำดับการอ่านโค้ด

```
src/
├── main.cpp                       ① จุดเริ่ม
├── engine/                        ส่วนที่ไม่รู้จัก "งู"
│   ├── core/Vec2.h                    struct ตำแหน่ง (x, y)
│   ├── core/Application.*         ② game loop
│   ├── platform/Terminal.*        ③ คุยกับ OS
│   ├── input/InputSystem.*        ④ แปลงปุ่ม
│   ├── render/FrameBuffer.*       ⑤ หน้าจอเสมือน
│   ├── render/TerminalRenderer.*  ⑤ เอาภาพขึ้นจอ
│   └── scene/Scene.h, SceneManager.*  ⑥ state machine
├── backend/redis/RedisClient.*    ⑫ คุยกับ Redis ผ่าน TCP (RESP)
└── game/                          ส่วนที่เป็นเกมงู
    ├── GameConfig.h, Session.h, PlayerName.h  ค่าตั้งต้น / ข้อมูลข้าม scene
    ├── sim/Direction.h, Snake.*, World.*  ⑦ กฎของเกม
    ├── services/Leaderboard.h + Redis/InMemory  ⑫ leaderboard
    └── scenes/Menu, NameEntry, Play, GameOver  ⑧ ตัวเชื่อม engine กับ sim
tests/TestMain.cpp                 ⑩ unit tests
```

อ่านตามลำดับเลขวงกลม ①→⑫ จะเห็นภาพจากบนลงล่าง

---

## 1. แบ่งชั้น (Layered Architecture)

```
┌──────────────────────────────┐
│ main.cpp                     │  ประกอบทุกอย่างเข้าด้วยกัน
├──────────────────────────────┤
│ game/scenes   (Play, Menu)   │  ตัวเชื่อม
├──────────────┬───────────────┤
│ game/sim     │ game/services │  กฎของเกม / leaderboard (interface + implementation)
├──────────────┴──────┬────────┤
│ engine (loop, input,│backend │  engine: ใช้กับเกมไหนก็ได้
│         render)     │(redis) │  backend: client ของ service ภายนอก
├─────────────────────┴────────┤
│ engine/platform (Terminal)   │  ชั้นเดียวที่แตะ OS
└──────────────────────────────┘
```

**กฎเหล็ก:** ชั้นล่างห้าม `#include` ชั้นบน

- `engine/` ไม่มีคำว่า snake เลยสักบรรทัด → เอาไปทำ Tetris ได้ทันที
- `game/sim/` ไม่ include `Terminal`, `FrameBuffer`, `InputSystem` → เอาไปรันบน server ที่ไม่มีจอได้
- ใน `CMakeLists.txt` แยกเป็น 3 library คือ `engine`, `backend`, `game` ให้ build system ช่วยบังคับขอบเขตนี้อีกชั้น (`engine` กับ `backend` ไม่รู้จักกัน)

**ในเกม AAA:** Unreal แบ่งเป็น `Core → Engine → Game module`, Unity แบ่ง `Engine (C++) → Scripting (C#)` หลักเดียวกัน

**ลองเอง:** `grep -r "snake" src/engine` ต้องไม่เจออะไรเลย

---

## 2. Game Loop และ Fixed Timestep

ไฟล์: `src/engine/core/Application.cpp`

ทุกเกมคือ loop นี้วนไปเรื่อย ๆ:

```
while (เล่นอยู่) {
    1. อ่าน input
    2. update โลกของเกม
    3. render
    4. รอให้ครบเวลาของ 1 เฟรม
}
```

### ปัญหา: ถ้า update ด้วยเวลาจริงที่ผ่านไป (variable dt)

```cpp
// แบบที่ "ไม่ควร" ทำ
double dt = now - previous;
world.update(dt);   // เครื่องเร็ว dt เล็ก, เครื่องช้า dt ใหญ่
```

- เครื่องช้า dt ใหญ่ → งูอาจกระโดดข้ามผลไม้ / ข้ามกำแพง
- ผลลัพธ์บนแต่ละเครื่อง **ไม่เหมือนกัน** → ทำ multiplayer หรือ replay ไม่ได้

### ทางแก้: Fixed Timestep ("Fix Your Timestep" โดย Glenn Fiedler)

```cpp
accumulator += std::min(elapsed, config_.maxFrameTime);
while (accumulator >= config_.fixedDt) {      // fixedDt = 1/60
    scene->update(config_.fixedDt);           // dt คงที่เสมอ
    accumulator -= config_.fixedDt;
}
```

คิดแบบนี้: `accumulator` คือ "หนี้เวลา" ที่ simulation ยังเดินตามไม่ทัน

- เฟรมนี้ผ่านไป 0.05 วิ → update 3 ครั้ง (3 × 0.0167) เหลือเศษ 0.0 วิ ไว้เฟรมหน้า
- เฟรมนี้ผ่านไป 0.01 วิ → update 0 ครั้ง สะสมไว้ก่อน
- `maxFrameTime = 0.25` กัน **spiral of death**: ถ้าเครื่องค้างไป 10 วิ จะไม่พยายาม update 600 ครั้งรวดจนค้างหนักกว่าเดิม

**ผล:** simulation เห็นเวลาเป็นก้อนละ 1/60 วินาทีเสมอ ไม่ว่า FPS จะเป็นเท่าไหร่

**ในงาน backend:** game server ส่วนใหญ่รันที่ "tick rate" คงที่ (เช่น CS2 = 64 tick, Valorant = 128 tick) คือแนวคิดเดียวกันเป๊ะ

**ลองเอง:** เปลี่ยน `targetFps` ใน `Application.h` เป็น 10 → ภาพกระตุก แต่ความเร็วงูเท่าเดิม นั่นคือหลักฐานว่า simulation ไม่ผูกกับ FPS

---

## 3. Platform Layer และ RAII

ไฟล์: `src/engine/platform/Terminal.cpp`

terminal ปกติทำงานแบบ "canonical mode": รอจนกด Enter ถึงส่งข้อความให้โปรแกรม และ echo ตัวอักษรที่พิมพ์ — ไม่เหมาะกับเกม เลยต้องเปลี่ยนเป็น **raw mode**:

```cpp
raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);  // ไม่รอ Enter, ไม่ echo, Ctrl+C เป็น byte ธรรมดา
raw.c_cc[VMIN] = 0;   // read() คืนค่าทันทีแม้ไม่มีข้อมูล (non-blocking)
raw.c_cc[VTIME] = 0;
```

และใช้ ANSI escape codes:

| Code | ความหมาย |
|---|---|
| `\x1b[?1049h` / `l` | เข้า/ออก alternate screen (จอแยก เกมปิดแล้วข้อความเดิมกลับมา) |
| `\x1b[?25l` / `h` | ซ่อน/โชว์ cursor |
| `\x1b[2J` | ล้างจอ |
| `\x1b[{row};{col}H` | ย้าย cursor |
| `\x1b[1;32m` | ตัวหนา สีเขียว |

### RAII: Resource Acquisition Is Initialization

ถ้าเกมจบแล้ว **ไม่** คืนค่า terminal ผู้ใช้จะได้ terminal ที่พิมพ์ไม่เห็นตัวอักษร จึงผูก "การคืนค่า" ไว้กับอายุของ object:

```cpp
Terminal::Terminal()  { /* เปิด raw mode */ }
Terminal::~Terminal() { restoreTerminal(); }  // ถูกเรียกเสมอเมื่อออกจาก scope
```

ใน `main.cpp` ถ้าเกิด exception ระหว่างเกม stack จะ unwind → destructor ของ `Terminal` ถูกเรียก → terminal กลับปกติ **ก่อน** ที่ `catch` จะพิมพ์ error

มีกันพลาดอีก 2 ชั้น: `std::atexit(restoreTerminal)` และจับสัญญาณ `SIGTERM`/`SIGHUP` ให้ออกจาก loop อย่างสุภาพ

**ในงาน backend:** RAII คือวิธีหลักในการจัดการ socket, DB connection, mutex lock (`std::lock_guard`) ไม่ให้รั่ว

---

## 4. Input: จาก byte สู่ Action

ไฟล์: `src/engine/input/InputSystem.cpp`

กดลูกศรขึ้น 1 ครั้ง terminal ส่งมา **3 byte**: `ESC [ A` (`\x1b[A`)

`parseInput()` แปลง byte เป็น **Action**:

```cpp
enum class Action { Up, Down, Left, Right, Confirm, Pause, Quit };
```

ทำไมไม่ให้เกมเช็ค `'w'` หรือ `\x1b[A` ตรง ๆ?

- **Rebinding**: เปลี่ยนปุ่มได้ที่เดียว เกมไม่ต้องแก้
- **หลาย input map เป็น action เดียว**: ลูกศรขึ้น, `w`, `W` → `Action::Up`
- **Testable**: `parseInput` เป็น pure function (รับ string คืน vector ไม่มี side effect) เทสได้โดยไม่ต้องมีคีย์บอร์ด

ข้อสังเกต: ESC เดี่ยว ๆ กับ ESC ที่เป็นหัวของลูกศร แยกกันได้เพราะเราอ่าน **ทุก byte ที่ค้างอยู่** ทีเดียวต่อเฟรม (`readAvailable`) ลูกศรมาครบ 3 byte พร้อมกัน

**ในเกม AAA:** Unreal Enhanced Input / Unity Input System ใช้แนวคิด Action Mapping แบบเดียวกัน

---

## 5. Rendering: FrameBuffer และ Delta Rendering

ไฟล์: `src/engine/render/FrameBuffer.*`, `TerminalRenderer.*`

### FrameBuffer = Back Buffer

scene ไม่ได้เขียนลงจอตรง ๆ แต่เขียนลง array ของ `Cell { glyph, color, bold }` ก่อน พอวาดครบทั้งเฟรมแล้วค่อย `present()` ทีเดียว → ไม่เห็นภาพวาดค้างครึ่ง ๆ (เหมือน double buffering ของ GPU)

`put()` ตัดส่วนที่หลุดจอทิ้งเอง (clipping) scene จึงไม่ต้องกังวลเรื่องวาดเกินขอบ

### Delta Rendering

```cpp
if (!fullRedraw && cell == previous_.at(x, y)) continue;  // cell ไม่เปลี่ยน ข้าม
```

จอ 80×30 = 2,400 ช่อง แต่ในหนึ่งเฟรมงูเปลี่ยนแค่ 2–3 ช่อง → ส่งข้อมูลลด ~99%

และรวมทุกอย่างเป็น `std::string` เดียว แล้ว `write()` ครั้งเดียวต่อเฟรม (syscall น้อย, ไม่กระพริบ)

### ทำไมแต่ละช่องกว้าง 2 คอลัมน์

ตัวอักษรใน terminal สูงประมาณ 2 เท่าของความกว้าง ถ้า 1 ช่อง = 1 ตัวอักษร งูจะวิ่งแนวตั้งเร็วกว่าแนวนอน (ด้วยสายตา) จึงวาดเป็น `"@ "` ให้ดูเป็นสี่เหลี่ยมจัตุรัส

**ในงาน backend:** นี่คือ **delta compression** ตัวเดียวกับที่ game server ใช้ส่ง snapshot ให้ client — ส่งเฉพาะสิ่งที่เปลี่ยนจาก snapshot ล่าสุดที่ client ยืนยันว่าได้รับ

---

## 6. Scene และ State Machine

ไฟล์: `src/engine/scene/Scene.h`, `SceneManager.*`

```
          Enter                  Enter              ตาย / ชนะ (หน่วง 1 วิ)
MenuScene ─────► NameEntryScene ─────► PlayScene ─────────────────► GameOverScene
    ▲  ◄── Esc ──────┘                    ▲                              │
    │                                     └────────── Enter ─────────────┤
    └──────────────────────────────────────────────── Esc ───────────────┘
```

แต่ละ scene implement 3 อย่าง: `handleInput`, `update(dt)`, `render(frame)`

### Deferred Scene Change

```cpp
void SceneManager::change(std::unique_ptr<Scene> next) { pending_ = std::move(next); }  // แค่จดไว้
void SceneManager::applyPending() { ... current_ = std::move(pending_); ... }          // สลับจริงตอนต้นเฟรม
```

ทำไมไม่สลับทันที? เพราะ `change()` ถูกเรียก **จากข้างใน** `PlayScene::update()` ถ้าสลับทันที `unique_ptr` จะ delete `PlayScene` ทิ้ง ขณะที่ method ของมันยังรันอยู่ → **use-after-free** (undefined behavior)

`PlayScene` ยังมี flag `leaving_` กันขอเปลี่ยน scene ซ้ำทุกเฟรมระหว่างรอ

### Session

`Session` (config, leaderboard, ชื่อผู้เล่น) อยู่ใน `main()` และส่งเป็น reference ให้ทุก scene → ข้อมูลอยู่รอดข้าม scene ได้ ขณะที่ `World` เกิดใหม่ทุกครั้งที่เริ่มเกม

---

## 7. Simulation: หัวใจของเกม

ไฟล์: `src/game/sim/`

นี่คือส่วนที่ **สำคัญที่สุด** สำหรับสาย backend

### กฎ 3 ข้อของ `World`

1. **ไม่มี I/O** — ไม่อ่านคีย์บอร์ด ไม่วาดจอ ไม่อ่านไฟล์
2. **ไม่อ่านเวลาจริง** — เวลาเดินเมื่อมีคนเรียก `step()` หรือ `advance(dt)` เท่านั้น
3. **สุ่มจาก seed** — `std::mt19937 rng_(seed)`

ผลคือ **deterministic**: seed เดียวกัน + command ชุดเดียวกัน ในลำดับเดียวกัน = ผลลัพธ์เหมือนกันทุกครั้ง

### Command เข้า, Event ออก

```
          TurnCommand                    GameEvent
ผู้เล่น ─────────────► World.step() ─────────────► UI / เสียง / analytics / network
          "ขอเลี้ยวขึ้น"                     "งูกินผลไม้ที่ (4,3)"
```

- **Command** = สิ่งที่ผู้เล่น *ขอ* (World อาจปฏิเสธ เช่น ขอกลับหลัง 180°)
- **Event** = สิ่งที่ *เกิดขึ้นแล้ว* (ข้อเท็จจริง)

`World` ไม่รู้ว่าใครฟัง event อยู่ — `PlayScene` ใช้มันปรับความเร็วและ high score วันหน้าจะต่อเสียง หรือส่งขึ้น server ก็ไม่ต้องแก้ `World`

### Snake: รายละเอียดที่ซ่อนอยู่

- `std::deque<Vec2i> body_` — เพิ่มหัว `push_front`, ตัดหาง `pop_back` ทั้งคู่ O(1)
- **Turn buffer** (สูงสุด 2): กด ขึ้น→ซ้าย เร็ว ๆ ภายใน tick เดียว การกดที่สองไม่หาย
- **ห้ามกลับหลัง**: เทียบกับทิศ *ล่าสุดในคิว* ไม่ใช่ทิศปัจจุบัน ไม่งั้นกด ขึ้น→ซ้าย→(ขวา) จะทะลุกฎได้
- **เดินเข้าช่องหางตัวเองได้**: `wouldCollide(cell, growing)` ไม่นับหาง ถ้าไม่ได้กำลังโต เพราะหางจะขยับออกใน tick เดียวกัน

### ลำดับใน `World::step()`

```
1. ใช้ turn ที่ค้างในคิว
2. คำนวณช่องถัดไป
3. หลุดขอบ? → กำแพงแข็ง: ตาย / กำแพงทะลุได้: wrap
4. ชนตัวเอง? → ตาย
5. ขยับงู
6. กินผลไม้? → +แต้ม, เช็ค wall event, สุ่มผลไม้ใหม่, ไม่มีที่ว่าง = ชนะ
```

### นาฬิกา 2 แบบ

| | `step()` | `advance(dt)` |
|---|---|---|
| หน่วย | tick (งูเดิน 1 ช่อง) | วินาที |
| ใครเรียก | `PlayScene` ทุก `moveInterval_` | `PlayScene` ทุก fixed update (1/60 วิ) |
| ใช้กับ | การเคลื่อนที่ ชน กิน | ตัวจับเวลา wall event |

ทำไมต้องแยก? เพราะความเร็วงูเปลี่ยนตามแต้ม ถ้านับ "กระพริบเตือน 2 วิ" เป็นจำนวน tick ความยาวจริงจะเปลี่ยนไปตามความเร็วงู ส่วน `advance` ยังคง deterministic ได้เพราะ `dt` ที่ส่งเข้ามาคงที่เสมอ

สังเกตว่าเงื่อนไขของ state machine เดียวกันมาจากคนละนาฬิกาได้: Warning → Solid ใช้ **เวลา** (`advance`) แต่ Solid → Open ใช้ **แต้ม** (`step` ตอนกินผลไม้)

---

## 8. ไล่ตามการกดปุ่ม 1 ครั้ง ตั้งแต่ต้นจนจบ

ผู้เล่นกด **ลูกศรขึ้น** ระหว่างเล่น:

```
1. Terminal::readAvailable()        ได้ "\x1b[A"
2. parseInput()                     → [Action::Up]
3. Application::run()               ไม่มี Quit → ส่งให้ scene
4. PlayScene::handleInput()         → world_.submit(TurnCommand{Up})
5. Snake::queueTurn(Up)             ไม่ใช่ทิศตรงข้าม → เข้าคิว
   ... fixed update หลายรอบ moveTimer_ สะสมจนครบ moveInterval_ ...
6. World::step()                    applyNextTurn() → direction_ = Up
                                    หัวใหม่ = head + (0,-1) → moveTo()
7. PlayScene::render()              วาด '@' ตำแหน่งใหม่ลง FrameBuffer
8. TerminalRenderer::present()      ส่งเฉพาะ ~3 ช่องที่เปลี่ยนไปที่ terminal
```

**ลองเอง:** ใส่ breakpoint (หรือ `std::cerr`) ที่แต่ละขั้น แล้วดูลำดับจริง

---

## 9. Case Study: การเพิ่ม Wall Event

feature นี้แสดงให้เห็นว่าสถาปัตยกรรมที่ดีทำให้เพิ่มของใหม่ได้ง่ายแค่ไหน

### State machine ของกำแพง

```
      score ≥ 200, 400, ...        2 วินาที             กินเพิ่มครบ 50 แต้ม
Open ─────────────────────► Warning ────────► Solid ─────────────────► Open
(ทะลุได้)                  (กระพริบ, ทะลุได้)  (แดง, ชน = ตาย)
                                      │
                                      └─ ถ้างูพาดข้ามขอบตอนนี้ → ตาย
```

ตอนเข้า Solid จะจด "แต้มเป้าหมาย" ไว้ (`wallUnlockAtScore_ = score_ + wallUnlockScore`) แล้วเช็คทุกครั้งที่กินผลไม้ใน `checkWallUnlock()`

### แก้ไฟล์ไหนบ้าง

| ชั้น | ไฟล์ | เพิ่มอะไร |
|---|---|---|
| Config | `GameConfig.h` | ตัวเลข 3 ค่า (data-driven) |
| Sim | `Snake.*` | `crossesEdge()` — ปล้องติดกันห่างเกิน 1 = พาดข้ามขอบ |
| Sim | `World.*` | `WallState`, `advance(dt)`, `checkWallUnlock`, event 3 ตัว, เงื่อนไขชนขอบ |
| Glue | `PlayScene.cpp` | เรียก `advance(dt)` + วาดสีกำแพง + HUD |
| Test | `TestMain.cpp` | 4 tests |
| Engine | — | **ไม่ต้องแตะเลย** |

### จุดที่ควรสังเกต

- `checkWallEventTrigger` ใช้ `while` ไม่ใช่ `if` เลื่อนเกณฑ์แต้ม — ถ้าแต้มกระโดดข้ามหลายเกณฑ์ จะไม่ trigger ค้างซ้ำ ๆ
- ถ้า event รอบก่อนยังไม่จบ รอบใหม่ถูกข้าม (ไม่ซ้อนกัน)
- ใน `step()` เรียก `checkWallUnlock` **ก่อน** `checkWallEventTrigger` — ถ้าผลไม้ลูกเดียวทั้งปลดล็อคและถึงเกณฑ์ถัดไปพร้อมกัน กำแพงจะเปิดแล้วเริ่มเตือนรอบใหม่ทันที (ถ้าสลับลำดับ รอบใหม่จะถูกข้ามเพราะยัง Solid อยู่)
- `kEpsilon = 1e-9` กันปัญหา floating point: บวก `1/60` 120 ครั้งอาจได้ `1.9999999999` ไม่ใช่ `2.0` พอดี
- pause แล้วเวลากำแพงหยุดด้วย อัตโนมัติ เพราะ `PlayScene::update` return ก่อนเรียก `advance`
- `render()` อ่าน `wallState()` อย่างเดียว ไม่แก้ state (กฎ: render ห้ามเปลี่ยน state)

---

## 10. Testing

ไฟล์: `tests/TestMain.cpp`

เพราะ `World` ไม่มี I/O จึงเทสได้แบบนี้:

```cpp
World world(config, /*seed*/ 1);
world.setFruitForTesting({4, 3});   // จัดฉากให้แน่นอน
world.step();
CHECK(world.wallState() == WallState::Warning);
world.advance(2.0);                 // กรอเวลา 2 วิ ในพริบตา
```

- ไม่ต้องเปิดจอ ไม่ต้องกดปุ่ม ไม่ต้องรอเวลาจริง
- รันทั้งชุดเสร็จในเสี้ยววินาที → ใส่ใน CI ได้

ใช้ test framework ทำเองแบบเล็ก ๆ (`CHECK` macro) เพื่อไม่ต้องพึ่ง library ภายนอก โปรเจกต์จริงนิยมใช้ GoogleTest หรือ Catch2

**ลองเอง:** เขียน test ว่า "กด ขึ้น แล้ว ซ้าย ภายใน tick เดียว งูต้องเลี้ยวครบทั้ง 2 ครั้งใน 2 tick ถัดไป"

---

## 11. จากเกมนี้ ไปสู่ Game Server

นี่คือเหตุผลที่โครงสร้างถูกออกแบบแบบนี้:

### Authoritative Server

```
Client A ── TurnCommand ──►┌─────────────────┐
                           │ Server          │
Client B ── TurnCommand ──►│  World.step()   │── snapshot / event ──► ทุก client
                           └─────────────────┘
```

client ส่งแค่ **command** server เป็นผู้ตัดสินผลเพียงคนเดียว → client โกงแต้มไม่ได้ เพราะไม่ได้เป็นคนคำนวณ

`World` ของเรายกไปใส่ server ได้ทันทีเพราะไม่มี I/O เลย

### Replay

บันทึกแค่ `seed + [(tick, command), ...]` ไฟล์เล็กมาก เล่นซ้ำได้ผลเหมือนเดิมเป๊ะเพราะ deterministic → ใช้ debug บั๊กที่ผู้เล่นรายงาน, ตรวจโกง, ทำ kill-cam

### Lockstep (RTS, เกมต่อสู้)

ทุกเครื่องรัน simulation เดียวกัน ส่งกันแค่ command → ใช้ bandwidth น้อยมาก แต่ต้อง deterministic 100% (นี่คือเหตุผลที่เราห่วงเรื่อง fixed dt และ seed)

> ข้อควรระวัง: `std::uniform_int_distribution` ให้ผลต่างกันได้ระหว่าง standard library (libc++ บน Mac vs libstdc++ บน Linux) ถ้าจะทำ cross-platform lockstep ต้องเขียนตัวสุ่มเอง

### Tick rate กับ Interpolation

server เดินที่ tick คงที่ (เหมือน `fixedDt`) ส่วน client render ที่ FPS สูงกว่า แล้ว interpolate ระหว่าง snapshot ให้ภาพลื่น — เกมนี้ไม่ได้ทำส่วน interpolation แต่เป็นหัวข้อถัดไปที่ควรศึกษา

---

## 12. Leaderboard ด้วย Redis

ไฟล์: `src/backend/redis/RedisClient.*`, `src/game/services/*`, `src/game/scenes/NameEntryScene.*`

บทนี้คืองาน backend จริง ๆ บทแรกของโปรเจกต์: เกมคุยกับ service ภายนอกผ่าน network

### 12.1 ภาพรวม

```
NameEntryScene ── ชื่อ ──► Session.playerName
                                  │
PlayScene ── (ตาย) ── submitScore(name, score) ──┐
MenuScene / GameOverScene ── top(5), standing() ─┤
                                                 ▼
                                   Leaderboard (interface)
                                   ┌─────────┴──────────┐
                           RedisLeaderboard     InMemoryLeaderboard
                                   │              (offline / test)
                           backend::redis::Client
                                   │  TCP + RESP
                                   ▼
                              Redis :6380
```

### 12.2 Dependency Inversion: scene ไม่รู้จัก Redis

`PlayScene` เรียก `session_.leaderboard.submitScore(...)` — `leaderboard` เป็น `Leaderboard&` ซึ่งเป็น **interface** (class ที่มีแต่ pure virtual function)

ใครเป็นตัวจริงข้างหลัง ตัดสินที่จุดเดียวใน `main.cpp`:

```cpp
auto redis = std::make_unique<snake::RedisLeaderboard>(host, port);
if (redis->connect()) return redis;                       // ต่อได้ -> Redis
return std::make_unique<snake::InMemoryLeaderboard>();    // ต่อไม่ได้ -> offline
```

ได้อะไร:

- **Graceful degradation**: Redis ล่ม เกมยังเล่นได้ (เกมจริงต้องเป็นแบบนี้เสมอ — leaderboard พังไม่ควรทำให้เข้าเกมไม่ได้)
- **เปลี่ยน backend ได้**: วันหน้าย้ายไปเรียก HTTP API ของ game server ก็เขียน `HttpLeaderboard` เพิ่มคลาสเดียว scene ไม่ต้องแก้
- **Test ได้**: `InMemoryLeaderboard` ใช้แทนของจริงใน test

### 12.3 Data model ใน Redis

| Key | Type | ทำไมเลือก type นี้ |
|---|---|---|
| `snake:leaderboard` | **Sorted Set** (ZSET) | เรียงลำดับให้อัตโนมัติ, หา rank ได้ O(log N) — เป็นโครงสร้างที่เกิดมาเพื่อ leaderboard |
| `snake:player:<name>` | **Hash** | เก็บหลาย field ของผู้เล่นคนเดียว (`games_played`, `last_score`) |

คำสั่งที่ใช้:

| ทำอะไร | คำสั่ง |
|---|---|
| ส่งคะแนน | `ZADD snake:leaderboard GT <score> <name>` |
| top 5 | `ZREVRANGE snake:leaderboard 0 4 WITHSCORES` |
| คะแนนดีสุดของฉัน | `ZSCORE snake:leaderboard <name>` |
| อันดับของฉัน | `ZREVRANK snake:leaderboard <name>` (เริ่มที่ 0 จึง +1) |
| นับเกม | `HINCRBY snake:player:<name> games_played 1` |

### 12.4 "ชื่อซ้ำ = คนเดิม" ทำงานยังไง

ใน Sorted Set, **member ไม่ซ้ำกัน** — `ZADD` ชื่อเดิมคือการอัปเดตคะแนนของ member เดิม ไม่ใช่เพิ่มแถวใหม่

แต่ถ้า `ZADD` ธรรมดา เล่นรอบหลังได้คะแนนน้อยกว่า คะแนนดีสุดจะถูกทับ → จึงใช้ flag **`GT`** (Redis 6.2+) = "อัปเดตเฉพาะเมื่อคะแนนใหม่มากกว่าเดิม" ทำในคำสั่งเดียว ไม่ต้องอ่านมาเทียบเองก่อน (ซึ่งจะเกิด race condition ถ้ามี 2 เครื่องส่งพร้อมกัน)

### 12.5 MULTI/EXEC + Pipelining

การส่งคะแนน 1 ครั้งต้องรัน 3 คำสั่ง:

```cpp
client_.pipeline({
    {"MULTI"},                                           // เริ่ม transaction
    {"ZADD", leaderboardKey(), "GT", value, player},
    {"HINCRBY", playerKey(player), "games_played", "1"},
    {"HSET", playerKey(player), "last_score", value},
    {"EXEC"},                                            // รันทั้งหมดแบบ atomic
});
```

- **MULTI/EXEC** = ทั้ง 3 คำสั่งรันติดกัน ไม่มีคำสั่งของ client อื่นแทรกกลาง
- **Pipelining** = เขียนทั้ง 5 คำสั่งลง socket ทีเดียว แล้วค่อยอ่านคำตอบ 5 ตัว → **1 round trip** แทน 5

ถ้า Redis อยู่อีกประเทศ (latency 100ms) ไม่ใช้ pipeline = 500ms, ใช้ = 100ms

### 12.6 RESP: โปรโตคอลของ Redis

เราไม่ได้ใช้ library (hiredis) แต่เขียน client เอง เพื่อให้เห็นว่าข้างใต้มันแค่ **ส่ง text ผ่าน TCP**

ทุกคำสั่งที่ส่งไป = Array ของ Bulk String:

```
ZSCORE snake:leaderboard bob
   ↓ encodeCommand()
*3\r\n$6\r\nZSCORE\r\n$17\r\nsnake:leaderboard\r\n$3\r\nbob\r\n
 │      │
 │      └─ $6 = string ยาว 6 byte ตามมา
 └─ *3 = array 3 ตัว
```

คำตอบขึ้นต้นด้วยตัวอักษรบอก type:

| ขึ้นต้น | Type | ตัวอย่าง |
|---|---|---|
| `+` | Status | `+OK\r\n` |
| `-` | Error | `-ERR syntax error\r\n` |
| `:` | Integer | `:3\r\n` |
| `$` | Bulk String | `$2\r\n80\r\n` (`$-1` = nil) |
| `*` | Array | `*2\r\n...` (ซ้อนกันได้) |

**ลองเอง:** `printf '*1\r\n$4\r\nPING\r\n' | nc 127.0.0.1 6380` จะได้ `+PONG` กลับมา — นี่คือสิ่งที่ `RedisClient` ทำทุกครั้ง

### 12.7 TCP stream ไม่มีขอบเขตข้อความ

จุดที่มือใหม่พลาดบ่อยที่สุดใน network programming: **`recv()` 1 ครั้ง ไม่ได้ = 1 ข้อความ**

- อาจได้ครึ่งข้อความ (`$5\r\nhel`)
- อาจได้หลายข้อความติดกัน (ตอน pipeline)

จึงต้องมี `buffer_` สะสม byte ไว้ และ `parseReply` คืน `nullopt` ถ้ายังไม่ครบ:

```cpp
while (replies.size() < commands.size()) {
    if (auto reply = parseReply(buffer_, consumed)) {  // parse ได้ -> ตัด byte ที่ใช้แล้วทิ้ง
        buffer_.erase(0, consumed);
        replies.push_back(*reply);
    } else if (!receiveMore()) {                      // ยังไม่ครบ -> recv เพิ่ม
        ...
    }
}
```

parser เป็น pure function จึงมี test เฉพาะเรื่องนี้: `redis: incomplete reply waits for more data`

### 12.8 Timeout ทุกจุด

เกมห้ามค้างเพราะ network ทุก I/O จึงมี timeout 300ms:

- `connect()` — ทำแบบ non-blocking แล้วรอด้วย `poll()` (connect ปกติอาจค้างนานถึง 75 วินาที)
- `send()` / `recv()` — ตั้ง `SO_SNDTIMEO` / `SO_RCVTIMEO`
- `TCP_NODELAY` — ปิด Nagle's algorithm ให้ส่ง packet เล็ก ๆ ทันที ไม่รอรวม
- `SO_NOSIGPIPE` / `MSG_NOSIGNAL` — ถ้า Redis ปิด connection ระหว่างส่ง โปรแกรมจะไม่ถูก signal SIGPIPE ฆ่าทิ้ง

ถ้า connection หลุด `RedisLeaderboard::ensureConnected()` จะลองต่อใหม่ในการเรียกครั้งถัดไปเอง

> **ข้อจำกัดที่ควรรู้:** การเรียก Redis ตอนนี้เป็นแบบ **blocking** บน game thread (เรียกแค่ตอนเปลี่ยน scene จึงไม่เห็นผล) เกมจริงจะทำ network I/O บน thread แยก หรือแบบ async — ดูแบบฝึกหัดข้อ 8

### 12.9 Input Context: ทำไมพิมพ์ `q` แล้วเกมไม่ปิด

ปกติ `q` = `Action::Quit` แต่ในหน้ากรอกชื่อ ต้องพิมพ์ `q` ได้ จึงมี **input mode**:

```cpp
class NameEntryScene : public Scene {
    bool wantsTextInput() const override { return true; }   // ขอโหมด Text
};
```

`Application` ถามทุกเฟรมว่า scene ปัจจุบันต้องการโหมดไหน แล้ว `parseInput(bytes, mode)` จะแปลตามโหมด:

| ปุ่ม | โหมด Game | โหมด Text |
|---|---|---|
| `q` | Quit | ตัวอักษร `q` |
| `w` `a` `s` `d` | เลี้ยว | ตัวอักษร |
| Space | Confirm | ตัวอักษร ` ` (ถูกกรองทิ้งเพราะชื่อห้ามมีช่องว่าง) |
| Enter / Esc / ลูกศร / Ctrl+C | action | action (เหมือนเดิม) |

**ในเกม AAA:** Unreal เรียกว่า Input Mapping Context — ตอนเปิดช่องแชท ปุ่ม WASD ต้องหยุดเดินตัวละครแล้วกลายเป็นตัวอักษร หลักเดียวกัน

### 12.10 Contract Test

test เดียว (`checkLeaderboardContract`) รันกับ **ทั้ง 2 implementation**:

```cpp
void testInMemoryLeaderboard() { InMemoryLeaderboard board; checkLeaderboardContract(board); }
void testRedisLeaderboard()    { RedisLeaderboard board(...prefix...); checkLeaderboardContract(board); }
```

รับประกันว่าโหมด offline กับ Redis ทำงานเหมือนกันจริง (เช่น เรียงลำดับคะแนนเท่ากันแบบเดียวกัน)

test Redis ใช้ key prefix `snake:test:<pid>:` แยกจากข้อมูลจริง และลบทิ้งหลังจบ — **ห้าม** test ลบ key ของข้อมูลจริงเด็ดขาด

---

## 13. แบบฝึกหัด

เรียงจากง่ายไปยาก

| # | งาน | คำใบ้ |
|---|---|---|
| 1 | ผลไม้ทอง: 10% ได้ 50 แต้ม | เพิ่ม type ใน fruit, ส่งใน `GameEvent` |
| 2 | `FileLeaderboard` เก็บคะแนนลงไฟล์ | implement `Leaderboard` เพิ่มอีกตัว แล้วรัน `checkLeaderboardContract` กับมัน — **ห้าม** เขียนไฟล์ใน `World` (ทำไม?) |
| 3 | Replay | บันทึก `(tick, command)` ใน `PlayScene`, สร้าง `ReplayScene` ป้อน command กลับเข้า `World` ด้วย seed เดิม |
| 4 | โหลด `GameConfig` จาก JSON | ใช้ nlohmann/json หรือเขียน parser ง่าย ๆ |
| 5 | Headless server | `server_main.cpp` รัน `World` + รับ command ผ่าน TCP socket, ให้ `snake` เป็น client |
| 6 | 2 ผู้เล่น | `World` มี `std::vector<Snake>`, `TurnCommand` มี `playerId` |
| 7 | ECS | แยก Snake/Fruit เป็น entity + component แล้วเปรียบเทียบความซับซ้อน |
| 8 | Async leaderboard | ย้ายการเรียก Redis ไป `std::thread` แยก ส่งงานผ่าน queue + `std::mutex` ให้ game loop ไม่ต้องรอ network |
| 9 | Leaderboard รายสัปดาห์ | ใช้ key `snake:leaderboard:2026-W46` + `EXPIRE` ให้ลบเองเมื่อหมดสัปดาห์ |
| 10 | กันโกง | ตอนนี้ใครก็ `ZADD` คะแนนปลอมได้ — ลองออกแบบให้ server รับ replay (seed + commands) แล้ว**รัน `World` ซ้ำเพื่อตรวจคะแนน**ก่อนบันทึก |

**คำถามชวนคิด**

1. ถ้าย้าย `std::random_device` เข้าไปใน `World` แทนรับ seed จากข้างนอก จะเสียอะไร?
2. ทำไม `render()` ห้ามแก้ state? ถ้าแก้จะเกิดอะไรเมื่อ FPS ต่างกัน?
3. ถ้าให้ `World::step()` เรียก `std::chrono::steady_clock::now()` จะทำ test ข้อไหนพัง?
4. ถ้า `SceneManager::change()` สลับ scene ทันที จะพังตรงไหนแน่ ๆ?
5. ถ้าไม่ใช้ `ZADD GT` แต่ทำ `ZSCORE` อ่านมาเทียบในโค้ด C++ ก่อนแล้วค่อย `ZADD` จะเกิดปัญหาอะไรถ้ามี 2 เครื่องส่งคะแนนชื่อเดียวกันพร้อมกัน?
6. ทำไม `submitScore` ถูกเรียกใน `PlayScene` ตอนเกิด event `SnakeDied` แทนที่จะเรียกใน `GameOverScene`?

---

## 14. คำศัพท์

| คำ | ความหมาย |
|---|---|
| **Tick** | หนึ่งก้าวของ simulation |
| **Fixed timestep** | update ด้วย dt คงที่เสมอ แยกจาก FPS |
| **Deterministic** | input เดียวกัน ได้ output เดียวกันทุกครั้ง |
| **Authoritative server** | server เป็นผู้ตัดสินผลเกม client แค่ส่งคำขอ |
| **Command** | คำขอจากผู้เล่น (อาจถูกปฏิเสธ) |
| **Event** | สิ่งที่เกิดขึ้นแล้วใน simulation |
| **Snapshot** | สำเนา state ของโลก ณ tick หนึ่ง |
| **Delta** | ส่วนต่างจาก snapshot ก่อนหน้า |
| **Back buffer** | หน้าจอเสมือนที่วาดให้เสร็จก่อนแสดง |
| **RAII** | ผูกการคืนทรัพยากรไว้กับ destructor |
| **State machine** | ระบบที่อยู่ได้ทีละ state และมีกฎการเปลี่ยน state ชัดเจน |
| **Spiral of death** | update ไม่ทันจนต้อง update มากขึ้น ยิ่งช้าลงเรื่อย ๆ |
| **Lockstep** | ทุกเครื่องรัน simulation เดียวกัน ส่งกันแค่ command |
| **Interface** | class ที่มีแต่ pure virtual function บอกว่า "ทำอะไรได้" ไม่บอกว่า "ทำยังไง" |
| **Dependency Inversion** | โค้ดระดับสูงพึ่ง interface ไม่พึ่ง implementation ตรง ๆ |
| **Graceful degradation** | ส่วนเสริมพัง ระบบหลักยังทำงานต่อได้ |
| **Sorted Set (ZSET)** | โครงสร้างของ Redis: member ไม่ซ้ำ + มี score + เรียงลำดับอัตโนมัติ |
| **RESP** | REdis Serialization Protocol — รูปแบบข้อความที่ Redis ใช้คุยผ่าน TCP |
| **Pipelining** | ส่งหลายคำสั่งรวดเดียวแล้วค่อยอ่านคำตอบ ลดจำนวน round trip |
| **Transaction (MULTI/EXEC)** | กลุ่มคำสั่งที่รันติดกันแบบ atomic |
| **Round trip** | เวลาส่งคำขอไปแล้วได้คำตอบกลับ 1 รอบ |
| **Contract test** | test ชุดเดียวที่รันกับทุก implementation ของ interface |
| **Input context** | ชุดการแปลปุ่มที่เปลี่ยนตามสถานการณ์ (เล่นเกม / พิมพ์ข้อความ) |
