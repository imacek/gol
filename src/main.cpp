#include "raylib.h"

#include <bitset>
#include <cstdint>
#include <format>
#include <thread>
#include <semaphore>
#include <mutex>
#include <atomic>

using namespace std;

constexpr int N = 2000;

struct World {
    bool Data[N][N];
};

void generateRandomNoise(World& world) {
    for (int x = 0; x < N; x++) {
        for (int y = 0; y < N; y++) {
            world.Data[x][y] = (rand() % 100) < 40;
        }
    }
}

int countAliveAround(const World& world, int x, int y) {
    int count = 0;

    constexpr int steps[][2] = {
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

    for (const auto & [dx, dy] : steps) {
        const int newX = x + dx;
        const int newY = y + dy;
        if (0 <= newX && newX < N && 0 <= newY && newY < N) {
            count += world.Data[newX][newY];
        } else {
            count += world.Data[(N + newX) % N][(N + newY) % N];
        }
    }

    return count;
}

void simulateLifeStep(const World& worldNow, World& worldNext, int minX = 0, int maxX = N) {
    for (int x = minX; x < maxX; x++) {
        for (int y = 0; y < N; y++) {
            const int count = countAliveAround(worldNow, x, y);

            if (worldNow.Data[x][y]) {
                worldNext.Data[x][y] = 2 <= count && count <= 3;
            } else {
                worldNext.Data[x][y] = count == 3;
            }
        }
    }
}

mutex worldPointersMutex;

int renderWorld = 0;
int simWorld1 = 0;
int simWorld2 = 1;
World worlds[3];

int simIndex = 0;
int frameIndex = 0;

constexpr int WORKER_COUNT = 10;

atomic<bool> killSwitch {false};
atomic<bool> workCanStart[WORKER_COUNT-1] {false};
atomic<int> workFinishedCount {0};

void simulateLoopWorker(const int wi) {
    const int minX = wi * N / WORKER_COUNT;
    const int maxX = (wi + 1) * N / WORKER_COUNT;

    while (!killSwitch) {
        if (bool yes = true; workCanStart[wi].compare_exchange_strong(yes, false)) {
            simulateLifeStep(worlds[simWorld1], worlds[simWorld2], minX, maxX);
            ++workFinishedCount;
        }
    }
}

void simulateLoop() {
    thread workers[WORKER_COUNT-1];
    for (int wi = 0; wi < WORKER_COUNT-1; wi++) {
        workers[wi] = thread{simulateLoopWorker, wi};
    }

    constexpr int last_wi = WORKER_COUNT-1;
    constexpr int minX = last_wi * N / WORKER_COUNT;
    constexpr int maxX = (last_wi + 1) * N / WORKER_COUNT;

    while (!killSwitch) {
        for (auto& wi : workCanStart) {
            wi.store(true);
        }

        simulateLifeStep(worlds[simWorld1], worlds[simWorld2], minX, maxX);

        while (!killSwitch && workFinishedCount < WORKER_COUNT-1) { }
        workFinishedCount = 0;

        {
            scoped_lock lock(worldPointersMutex);

            ++simIndex;

            simWorld1 = simWorld2;

            // get empty tick
            if (renderWorld != 0 && simWorld1 != 0) {
                simWorld2 = 0;
            } else if (renderWorld != 1 && simWorld1 != 1) {
                simWorld2 = 1;
            } else {
                simWorld2 = 2;
            }
        }
    }

    for (auto& worker : workers) {
        worker.join();
    }
}

int main() {
    // Init Sim World
    generateRandomNoise(worlds[simWorld1]);

    InitWindow(N, N, "Game Of Life");
    //SetTargetFPS(64);

    thread simThread(simulateLoop);

    const Image img = GenImageColor(N, N, BLACK);
    const Texture2D tex = LoadTextureFromImage(img);

    int localSimIndex = 0;

    while (!WindowShouldClose()) {

        {
            scoped_lock lock(worldPointersMutex);
            renderWorld = simWorld1;
        }

        localSimIndex = simIndex;

        const auto& [Data] = worlds[renderWorld];

        for (int x = 0; x < N; x++) {
            for (int y = 0; y < N; y++) {
                static_cast<Color*>(img.data)[x*N + y] = Data[x][y] ? RED : DARKGREEN;
            }
        }

        BeginDrawing();

        UpdateTexture(tex, img.data);

        DrawTexture(tex, 0, 0, WHITE);

        string simDetails = format("FPS {}\tFID {}\nSPS X\tSID {} (d {})", GetFPS(), frameIndex, localSimIndex, localSimIndex-frameIndex);
        DrawText(simDetails.c_str(), 0, 0, 30, BLACK);

        EndDrawing();

        ++frameIndex;
    }

    killSwitch = true;

    UnloadTexture(tex);
    UnloadImage(img);

    simThread.join();

    CloseWindow();

    return 0;
}
