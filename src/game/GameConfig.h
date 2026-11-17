#pragma once

#include <string>
#include <vector>

namespace snake {

// ค่า tuning ทั้งหมดรวมไว้ที่เดียว (data-driven) — ในเกมจริงมักโหลดจากไฟล์ JSON / server
struct GameConfig {
    int boardWidth = 24;
    int boardHeight = 16;
    int initialLength = 3;
    bool wrapWalls = true;  // true = ทะลุขอบไปโผล่อีกฝั่ง, false = ชนขอบตาย

    int scorePerFruit = 10;
    double startMoveInterval = 0.15;  // วินาทีต่อ 1 ช่อง
    double minMoveInterval = 0.06;
    double speedupPerFruit = 0.004;

    // Wall event: ทุก ๆ N แต้ม กำแพงกระพริบเตือน (ยังทะลุได้) แล้วกลายเป็นกำแพงแดง (ชน = ตาย)
    // กำแพงแดงจะปลดล็อคเมื่อเก็บแต้มเพิ่มได้ครบ wallUnlockScore
    int wallEventScoreInterval = 200;  // 0 = ปิด event นี้
    double wallWarningDuration = 2.0;
    int wallUnlockScore = 50;

    // Typing Challenge: กิน Challenge Fruit แล้วต้องพิมพ์คำให้ทันเวลา ผ่าน = ได้ Life
    // ค่าเวลาทั้งหมดจูนจากการลองเล่น prototype ก่อนเขียนจริง
    std::vector<std::string> challengeWords = {
        "fang", "coil",  "tail",  "hiss",   "venom", "scale", "viper", "cobra", "apple", "fruit",
        "shed", "mamba", "python", "adder", "slink", "prey",  "bite",  "curl",  "glide",
    };
    double challengeSecondsPerLetter = 1.0;
    double challengeSpeedFactor = 0.5;  // งูเดินช้าลงเหลือครึ่งเดียวระหว่าง challenge
    double challengeFruitInterval = 20.0;  // นับเฉพาะเวลาเล่น เริ่มนับใหม่เมื่อถูกกินหรือหายไป
    double challengeFruitLifetime = 5.0;
    double challengeFruitBlinkSeconds = 2.0;  // กะพริบเตือนช่วงท้ายก่อนหายไป
    int maxLives = 3;
    int fullLivesBonusScore = 20;  // ผ่าน challenge ตอน Life เต็ม
    double invulnerabilitySeconds = 2.0;  // หลังเสีย Life ชนอะไรก็ไม่ตาย
};

}  // namespace snake
