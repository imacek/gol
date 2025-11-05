#include <cstdio>
#include "raylib.h"

#include <bitset>
#include <cstdint>
#include <format>
#include <thread>
#include <semaphore>

using namespace std;

constexpr int SIZE = 2000;

struct World {
    bool Data[SIZE][SIZE];
};

void generateRandomNoise(World& world) {
    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < SIZE; y++) {
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
        if (0 <= newX && newX < SIZE && 0 <= newY && newY < SIZE) {
            count += world.Data[newX][newY];
        }
    }

    return count;
}

void simulateLifeStep(const World& worldNow, World& worldNext) {
    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < SIZE; y++) {
            int count = countAliveAround(worldNow, x, y);

            if (worldNow.Data[x][y]) {
                worldNext.Data[x][y] = 2 <= count && count <= 3;
            } else {
                worldNext.Data[x][y] = count == 3;
            }
        }
    }
}

binary_semaphore semaphore{1};

int currentWorldIndex = 0;
World worlds[2] = {};

void simulateLoop() {
    while (!WindowShouldClose()) {

        semaphore.acquire();
        const World& worldNow = worlds[currentWorldIndex];

        currentWorldIndex = (currentWorldIndex + 1) % 2;
        World& worldNext = worlds[currentWorldIndex];

        simulateLifeStep(worldNow, worldNext);
    }
}

int main() {
    generateRandomNoise(worlds[currentWorldIndex]);
    worlds[1] = worlds[0];

    InitWindow(SIZE, SIZE, "Game Of Life");

    //SetTargetFPS(64);

    Image img = GenImageColor(SIZE, SIZE, DARKGREEN);
    const Texture2D tex = LoadTextureFromImage(img);

    thread simThread(simulateLoop);

    while (!WindowShouldClose()) {

        World& world = worlds[currentWorldIndex];

        semaphore.release();

        for (int x = 0; x < SIZE; x++) {
            for (int y = 0; y < SIZE; y++) {
                //ImageDrawPixel(&img, x, y, world.Data[x][y] ? RED : DARKGREEN);
                static_cast<Color*>(img.data)[x*SIZE + y] = world.Data[x][y] ? RED : DARKGREEN;
            }
        }

        BeginDrawing();
        ClearBackground(WHITE);

        UpdateTexture(tex, img.data);

        DrawTexture(tex, 0, 0, WHITE);

        string fps = format("FPS {}", GetFPS());
        DrawText(fps.c_str(), 0, 0, 30, BLACK);

        EndDrawing();
    }

    UnloadTexture(tex);
    UnloadImage(img);

    CloseWindow();

    return 0;
}
