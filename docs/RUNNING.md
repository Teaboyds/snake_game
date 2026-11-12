# วิธีรันเกม Snake

## 1. สิ่งที่ต้องมี

| อย่าง | เวอร์ชัน | ติดตั้งบน macOS |
|---|---|---|
| C++ compiler (clang / gcc) | รองรับ C++17 | `xcode-select --install` |
| CMake | 3.16 ขึ้นไป | `brew install cmake` |
| Terminal | ขนาดอย่างน้อย **51 × 30** ตัวอักษร | Terminal.app / iTerm2 / VS Code terminal |
| Redis (ไม่บังคับ) | 6.2 ขึ้นไป | ดูหัวข้อ "Leaderboard (Redis)" ด้านล่าง |

รองรับ **macOS และ Linux** เท่านั้น (Windows ยังไม่รองรับ เพราะใช้ `termios`)

ตรวจว่าพร้อมแล้ว:

```bash
clang++ --version
cmake --version
```

## 2. Build

```bash
cd /Users/patiphan/Desktop/project/snake_c++
cmake -S . -B build        # ครั้งแรกครั้งเดียว: สร้างไฟล์ build ในโฟลเดอร์ build/
cmake --build build -j     # compile (รันทุกครั้งที่แก้โค้ด)
```

ได้ไฟล์ออกมา 2 ตัว:

- `build/snake` — ตัวเกม
- `build/snake_tests` — unit tests

### Build แบบ Release (เร็วกว่า, ไม่มี debug info)

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
./build-release/snake
```

## 3. เล่นเกม

```bash
./build/snake
```

ลำดับหน้าจอ: **Menu** (Enter) → **กรอกชื่อ** (Enter) → **เล่น** → **Game Over** (Enter เล่นใหม่ด้วยชื่อเดิม / Esc กลับเมนู)

### กรอกชื่อ

- ใช้ได้เฉพาะ `A-Z a-z 0-9 _ -` ยาวไม่เกิน 12 ตัว (ภาษาไทยยังไม่รองรับ)
- **ชื่อซ้ำ = ผู้เล่นคนเดิม** — คะแนนดีที่สุดและจำนวนเกมจะสะสมรวมกัน
- ตัวพิมพ์เล็ก/ใหญ่ถือเป็นคนละชื่อ (`Bob` ≠ `bob`)
- Backspace ลบ, Esc กลับเมนู, Ctrl+C ออกจากเกม (ตัว `q` พิมพ์เป็นชื่อได้ ไม่ปิดเกม)

### ปุ่ม

| ปุ่ม | ทำอะไร |
|---|---|
| ลูกศร หรือ W A S D | เลี้ยว |
| Enter / Space | เริ่มเกม / เล่นใหม่ |
| P หรือ Esc | หยุดชั่วคราว (ในหน้า Game Over: Esc = กลับเมนู) |
| Q หรือ Ctrl+C | ออกจากเกม |

### กติกา

- กินผลไม้ `*` ได้ **+10 แต้ม** งูยาวขึ้น 1 ช่อง และวิ่งเร็วขึ้นเล็กน้อย
- **ชนตัวเอง = ตาย**
- ปกติขอบจอ **ทะลุได้** (ออกขวาไปโผล่ซ้าย)
- **Wall event** ทุก ๆ 200 แต้ม:
  1. กำแพงกระพริบแดง **2 วินาที** (ยังทะลุได้) → HUD ขึ้น `! WALLS CLOSING !`
  2. ถ้าตอนกำแพงเปลี่ยนเป็นแดง ตัวงูยังพาดข้ามขอบอยู่ → **ตาย**
  3. กำแพงแดง ชนแล้วตาย → HUD ขึ้น `LOCKED! eat +50 to unlock`
  4. ต้อง **กินผลไม้ให้ได้อีก 50 แต้ม** (5 ลูก) กำแพงจึงจะปลดล็อคกลับมาทะลุได้ — รอเฉย ๆ ไม่หาย
- ถ้างูยาวเต็มกระดาน = ชนะ

### ปรับค่าเกม

แก้ที่ `src/game/GameConfig.h` แล้ว build ใหม่ (`cmake --build build -j`)

| ค่า | ค่าเริ่มต้น | ความหมาย |
|---|---|---|
| `boardWidth`, `boardHeight` | 24, 16 | ขนาดกระดาน |
| `initialLength` | 3 | ความยาวงูตอนเริ่ม |
| `wrapWalls` | `true` | `false` = ชนขอบตายตลอดเวลา |
| `scorePerFruit` | 10 | แต้มต่อผลไม้ |
| `startMoveInterval` | 0.15 | วินาทีต่อการเดิน 1 ช่องตอนเริ่ม (ยิ่งน้อยยิ่งเร็ว) |
| `minMoveInterval` | 0.06 | ความเร็วสูงสุด |
| `speedupPerFruit` | 0.004 | เร็วขึ้นเท่าไหร่ต่อผลไม้ 1 ลูก |
| `wallEventScoreInterval` | 200 | เกิด wall event ทุกกี่แต้ม (`0` = ปิด) |
| `wallWarningDuration` | 2.0 | เวลากระพริบเตือน (วินาที) |
| `wallUnlockScore` | 50 | แต้มที่ต้องเก็บเพิ่มระหว่างกำแพงแดงเพื่อปลดล็อค |

> ทริค: อยากทดสอบ wall event เร็ว ๆ ตั้ง `wallEventScoreInterval = 20`

## 4. Leaderboard (Redis)

เกมเก็บคะแนนไว้ใน Redis ถ้าต่อไม่ได้ เกมยังเล่นได้ปกติในโหมด offline (คะแนนหายเมื่อปิดเกม) — ดูสถานะได้ที่บรรทัดล่างสุดของหน้า Menu

### ตั้งค่าการเชื่อมต่อ

ค่าเริ่มต้นคือ `127.0.0.1:6380` เปลี่ยนได้ด้วย environment variable:

```bash
SNAKE_REDIS_HOST=127.0.0.1 SNAKE_REDIS_PORT=6379 ./build/snake
```

### เปิด Redis (ถ้ายังไม่มี)

```bash
docker run -d --name snake-redis -p 6380:6379 redis:7-alpine
# หรือ
brew install redis && redis-server --port 6380
```

### ดูข้อมูลใน Redis

```bash
redis-cli -p 6380 ZREVRANGE snake:leaderboard 0 9 WITHSCORES   # top 10
redis-cli -p 6380 HGETALL snake:player:<ชื่อ>                   # สถิติของผู้เล่น
redis-cli -p 6380 ZREM snake:leaderboard <ชื่อ>                 # ลบผู้เล่นคนเดียวออกจากอันดับ
```

| Key | Type | เก็บอะไร |
|---|---|---|
| `snake:leaderboard` | Sorted Set | member = ชื่อ, score = คะแนนดีที่สุด |
| `snake:player:<ชื่อ>` | Hash | `games_played`, `last_score` |

## 5. รัน Tests

```bash
./build/snake_tests
# หรือผ่าน CTest
ctest --test-dir build --output-on-failure
```

ผลที่ควรเห็น: ทุกบรรทัดเป็น `[PASS]` และปิดท้ายด้วย `All tests passed`

test `leaderboard: redis (live)` จะต่อ Redis จริง (ใช้ key prefix `snake:test:<pid>:` แล้วลบทิ้งเอง ไม่แตะข้อมูลเกมจริง) ถ้า Redis ไม่เปิดจะขึ้น `[SKIP]`

## 6. แก้ปัญหา

| อาการ | สาเหตุ / วิธีแก้ |
|---|---|
| `fatal: must be run in an interactive terminal` | รันจาก output panel ของ IDE หรือ pipe ข้อมูลเข้า → เปิดใน terminal จริง |
| ขึ้น `Terminal too small - please enlarge` | ขยายหน้าต่าง terminal ให้ได้อย่างน้อย 51 × 30 หรือลด `boardWidth`/`boardHeight` |
| Menu ขึ้น `leaderboard: offline` สีแดง | ต่อ Redis ไม่ได้ → เช็คว่า Redis เปิดอยู่ (`redis-cli -p 6380 ping` ต้องได้ `PONG`) และ port ตรงกับ `SNAKE_REDIS_PORT` — ข้อความ error จริงจะพิมพ์ออกมาหลังปิดเกม |
| Menu ขึ้น `(disconnected)` | Redis ล่มระหว่างเล่น เกมจะลองต่อใหม่เองเมื่อต้องอ่าน/เขียนคะแนนครั้งถัดไป |
| `ERR syntax error` ใน Redis log | Redis เก่ากว่า 6.2 ไม่รู้จัก `ZADD GT` → อัปเกรด Redis |
| ลูกศรไม่ทำงาน | บาง terminal ส่งรหัสปุ่มต่างไป → ใช้ W A S D แทน |
| หลังเกมแครช terminal พิมพ์ไม่ขึ้น / เพี้ยน | พิมพ์ `reset` แล้วกด Enter (พิมพ์ไม่เห็นก็พิมพ์ไปเลย) |
| `cmake: command not found` | `brew install cmake` |
| build error แปลก ๆ หลังย้ายโฟลเดอร์ / เปลี่ยน compiler | ลบแล้ว build ใหม่: `rm -rf build && cmake -S . -B build && cmake --build build -j` |
| ตัวอักษรกระพริบ / ภาพแตก | ลองใช้ iTerm2 หรือขยายหน้าต่างใหม่ (เกมจะวาดใหม่ทั้งจอเมื่อขนาดเปลี่ยน) |
