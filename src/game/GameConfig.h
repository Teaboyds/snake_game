#pragma once

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
};

}  // namespace snake
