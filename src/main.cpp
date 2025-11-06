#include "raylib.h"

#include <bitset>
#include <cstdint>
#include <format>
#include <thread>
#include <semaphore>
#include <mutex>
#include <condition_variable>

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

    for (const auto & [dx, dy] : steps) {
        const int newX = x + dx;
        const int newY = y + dy;
        if (0 <= newX && newX < SIZE && 0 <= newY && newY < SIZE) {
            count += world.Data[newX][newY];
        } else {
            count += world.Data[(SIZE + newX) % SIZE][(SIZE + newY) % SIZE];
        }
    }

    return count;
}

void simulateLifeStep(const World& worldNow, World& worldNext, int minX = 0, int maxX = SIZE) {
    for (int x = minX; x < maxX; x++) {
        for (int y = 0; y < SIZE; y++) {
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

constexpr int WORKER_COUNT = 16;
mutex workerMutex[WORKER_COUNT];
condition_variable workerCondVar[WORKER_COUNT];
bool workerHasWork[WORKER_COUNT] = { false };

struct default_binary_semaphore
{
    binary_semaphore sem{0}; //Make it public for easy access to the semaphore

    constexpr default_binary_semaphore() : sem (0) {}
    constexpr explicit default_binary_semaphore(auto count) : sem(count) {}
};
default_binary_semaphore workerStartSemaphores[WORKER_COUNT];
default_binary_semaphore workerFinishSemaphores[WORKER_COUNT];

void simulateLoopWorker(const int wi) {
    const int minX = wi * SIZE / WORKER_COUNT;
    const int maxX = (wi + 1) * SIZE / WORKER_COUNT;

    while (!WindowShouldClose()) {

        workerStartSemaphores[wi].sem.acquire();
        // {
        //     unique_lock lock(workerMutex[wi]);
        //     workerCondVar[wi].wait(lock, [wi] { return workerHasWork[wi]; });
        // }

        simulateLifeStep(worlds[simWorld1], worlds[simWorld2], minX, maxX);

        workerFinishSemaphores[wi].sem.release();
        // {
        //     lock_guard lock(workerMutex[wi]);
        //     workerHasWork[wi] = false;
        //     workerCondVar[wi].notify_one();
        // }
    }

    workerFinishSemaphores[wi].sem.release();
}

void simulateLoop() {

    thread workers[WORKER_COUNT];
    for (int wi = 0; wi < WORKER_COUNT; wi++) {
        workers[wi] = thread{simulateLoopWorker, wi};
    }

    while (!WindowShouldClose()) {

        for (int wi = 0; wi < WORKER_COUNT; wi++) {
            workerStartSemaphores[wi].sem.release();
        }

        // for (int wi = 0; wi < WORKER_COUNT; wi++) {
        //     lock_guard lock(workerMutex[wi]);
        //     workerHasWork[wi] = true;
        //     workerCondVar[wi].notify_one();
        // }

        for (int wi = 0; wi < WORKER_COUNT; wi++) {
            // unique_lock lock(workerMutex[wi]);
            // workerCondVar[wi].wait(lock, [wi] { return !workerHasWork[wi]; });
            workerFinishSemaphores[wi].sem.acquire();
        }

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

    for (int wi = 0; wi < WORKER_COUNT; wi++) {
        workerStartSemaphores[wi].sem.release();
    }

    for (auto& worker : workers) {
        worker.join();
    }
}

int main() {
    // Init Sim World
    generateRandomNoise(worlds[simWorld1]);

    InitWindow(SIZE, SIZE, "Game Of Life");
    //SetTargetFPS(64);

    thread simThread(simulateLoop);

    const Image img = GenImageColor(SIZE, SIZE, BLACK);
    const Texture2D tex = LoadTextureFromImage(img);

    int localSimIndex = 0;

    while (!WindowShouldClose()) {

        {
            scoped_lock lock(worldPointersMutex);
            renderWorld = simWorld1;

            localSimIndex = simIndex;
        }

        const auto& [Data] = worlds[renderWorld];

        for (int x = 0; x < SIZE; x++) {
            for (int y = 0; y < SIZE; y++) {
                static_cast<Color*>(img.data)[x*SIZE + y] = Data[x][y] ? RED : DARKGREEN;
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

    UnloadTexture(tex);
    UnloadImage(img);

    simThread.join();

    CloseWindow();

    return 0;
}
