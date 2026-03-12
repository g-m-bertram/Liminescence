#include "Liminescence.h"
#include"raylib.h"
#include"World.h"


int main()
{
    InitWindow(800, 600, "Liminescence");
    SetTargetFPS(60);

    Camera3D camera = {};
    camera.position = { 0.f, 5.f, 10.f };
    camera.target = { 0.f, 0.f, 0.f };
    camera.up = { 0.f, 1.f, 0.f };
    camera.fovy = 70.f;
    camera.projection = CAMERA_PERSPECTIVE;

    DisableCursor();

    World world;

    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_FREE);

        BeginDrawing();

        ClearBackground(SKYBLUE);

        BeginMode3D(camera);
            DrawGrid(20, 1.f);
            world.Draw();
        EndMode3D();

        DrawText("WASD to move, mouse to look", 10, 10, 20, BLACK);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}