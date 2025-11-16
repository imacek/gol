#include "raylib.h"

#include <bitset>
#include <cstdint>
#include <format>
#include <thread>
#include <semaphore>
#include <mutex>
#include <atomic>
#include <cmath>
#include <iostream>

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


mutex worldPointersMutex;

struct WorldIndices {
    uint8_t render = 0;
    uint8_t simOld = 0;
    uint8_t simNext = 1;
};

World worlds[3];
WorldIndices worldIndices;

void simulateLifeStep(const int minX = 0, const int maxX = N) {
    const World& worldNow = worlds[worldIndices.simOld];
    World& worldNext = worlds[worldIndices.simNext];

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


int simIndex = 0;
int frameIndex = 0;

chrono::time_point<chrono::high_resolution_clock> lastSimTime;
float simDuration = 0.f;

int GetSPS()
{
    constexpr int SPS_CAPTURE_FRAMES_COUNT = 30;        // 30 captures
    constexpr float SPS_AVERAGE_TIME_SECONDS = 0.5f;    // 500 milliseconds
    constexpr float SPS_STEP = SPS_AVERAGE_TIME_SECONDS / SPS_CAPTURE_FRAMES_COUNT;

    static int index = 0;
    static float history[SPS_CAPTURE_FRAMES_COUNT] = { 0 };
    static float average = 0;
    static chrono::time_point<chrono::high_resolution_clock> last;

    if (simDuration == 0) return 0;

    const chrono::time_point<chrono::high_resolution_clock> now = chrono::high_resolution_clock::now();

    if (const chrono::duration<float> sinceLastCheck = (now - last); sinceLastCheck.count() > SPS_STEP)
    {
        last = now;
        index = (index + 1) % SPS_CAPTURE_FRAMES_COUNT;
        average -= history[index];
        history[index] = simDuration / SPS_CAPTURE_FRAMES_COUNT;
        average += history[index];
    }

    return static_cast<int>(roundf(1.0f / average));
}


constexpr int WORKER_COUNT = 10;

atomic<bool> killSwitch {false};
atomic<bool> workCanStart[WORKER_COUNT-1] {false};
atomic<int> workFinishedCount {0};

void simulateLoopWorker(const int wi) {
    const int minX = wi * N / WORKER_COUNT;
    const int maxX = (wi + 1) * N / WORKER_COUNT;

    while (!killSwitch) {
        if (bool yes = true; workCanStart[wi].compare_exchange_strong(yes, false)) {
            simulateLifeStep(minX, maxX);
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

        simulateLifeStep(minX, maxX);

        while (!killSwitch && workFinishedCount < WORKER_COUNT-1) { }
        workFinishedCount = 0;

        {
            scoped_lock lock(worldPointersMutex);

            ++simIndex;

            const chrono::time_point<chrono::high_resolution_clock> now = chrono::high_resolution_clock::now();
            chrono::duration<float> durInSeconds {now - lastSimTime};
            simDuration = durInSeconds.count();
            lastSimTime = now;

            worldIndices.simOld = worldIndices.simNext;

            if (worldIndices.render != 0 && worldIndices.simOld != 0) {
                worldIndices.simNext = 0;
            } else if (worldIndices.render != 1 && worldIndices.simOld != 1) {
                worldIndices.simNext = 1;
            } else {
                worldIndices.simNext = 2;
            }
        }
    }

    for (auto& worker : workers) {
        worker.join();
    }
}

int main() {
    //cout << "LOCK " << worldIndices.is_lock_free() << endl;

    // Init Sim World
    generateRandomNoise(worlds[worldIndices.simOld]);

    InitWindow(N, N, "Game Of Life");
    SetTargetFPS(64);

    thread simThread(simulateLoop);

    const Image img = GenImageColor(N, N, BLACK);
    const Texture2D tex = LoadTextureFromImage(img);

    int localSimIndex = 0;

    while (!WindowShouldClose()) {

        {
            scoped_lock lock(worldPointersMutex);
            worldIndices.render = worldIndices.simOld;
        }

        localSimIndex = simIndex;

        const auto Data = worlds[worldIndices.render].Data;

        for (int x = 0; x < N; x++) {
            for (int y = 0; y < N; y++) {
                static_cast<Color*>(img.data)[x*N + y] = Data[x][y] ? RED : DARKGREEN;
            }
        }

        BeginDrawing();

        UpdateTexture(tex, img.data);

        DrawTexture(tex, 0, 0, WHITE);


        string simDetails = format("FPS {}\tFID {}\nSPS {}\tSID {} (d {})",
            GetFPS(), frameIndex, GetSPS(), localSimIndex, localSimIndex-frameIndex);
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
