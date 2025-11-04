#include <cstdio>
#include "raylib.h"

#include <bitset>
#include <cstdint>
#include <format>
#include <thread>

using namespace std;

struct World {
    bool Data[1000][1000] = {false};
};

void generateRandomNoise(World& world) {
    for (int x = 0; x < 1000; x++) {
        for (int y = 0; y < 1000; y++) {
            world.Data[x][y] = (rand() % 100) < 40;
        }
    }
}

int countAliveAround(const World& world, int x, int y) {
    int count = 0;

    static int steps[][2] = {
        { -1, -1 },
        { -1,  0 },
        { -1,  1 },
        {  0, -1 },
        // SKIP {  0,  0 },
        {  0,  1 },
        {  1, -1 },
        {  1,  0 },
        {  1,  1 },
    };

    for (const auto& [dx, dy] : steps) {
        const int newX = x + dx;
        const int newY = y + dy;
        if (0 <= newX && newX < 1000 && 0 <= newY && newY < 1000) {
            count += world.Data[newX][newY];
        }
    }

    return count;
}

void simulateLifeStep(const World& worldNow, World& worldNext) {
    for (int x = 0; x < 1000; x++) {
        for (int y = 0; y < 1000; y++) {
            int count = countAliveAround(worldNow, x, y);

            if (worldNow.Data[x][y]) {
                worldNext.Data[x][y] = 2 <= count && count <= 3;
            } else {
                worldNext.Data[x][y] = count == 3;
            }
        }
    }
}

int currentWorldIndex = 0;
World worlds[2] = {};

void simulateLoop() {
    const World& worldNow = worlds[currentWorldIndex];

    currentWorldIndex = (currentWorldIndex + 1) % 2;
    World& worldNext = worlds[currentWorldIndex];

    simulateLifeStep(worldNow, worldNext);
}

int main() {
    generateRandomNoise(worlds[currentWorldIndex]);
    worlds[1] = worlds[0];

    InitWindow(1000, 1000, "Game Of Life");
//    SetTargetFPS(64);

    Image img = GenImageColor(1000, 1000, DARKGREEN);
    const Texture2D tex = LoadTextureFromImage(img);

    while (!WindowShouldClose()) {

        //simulateLoop();
        thread simThread(simulateLoop);

        World& worldNext = worlds[currentWorldIndex];

        for (int x = 0; x < 1000; x++) {
            for (int y = 0; y < 1000; y++) {
                ImageDrawPixel(&img, x, y, worldNext.Data[x][y]? RED : DARKGREEN);
            }
        }

        BeginDrawing();
        ClearBackground(WHITE);

        UpdateTexture(tex, img.data);

        DrawTexture(tex, 0, 0, WHITE);

        string fps = format("FPS {}", 1. / GetFrameTime());
        DrawText(fps.c_str(), 0, 0, 30, BLACK);

        EndDrawing();

        simThread.join();
    }

    UnloadTexture(tex);
    UnloadImage(img);

    CloseWindow();

    return 0;
}
