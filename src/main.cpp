#include "Liminescence.h"
#include "raylib.h"

int main()
{
    InitWindow(800, 600, "My Game");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Raylib is working!", 300, 280, 20, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}