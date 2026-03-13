#include "Liminescence.h"
#include"raylib.h"
#include"World.h"
#include"Player.h"


int main()
{
    InitWindow(800, 600, "Liminescence");
    SetTargetFPS(60);
    DisableCursor();

    World world;
    Player player({ 8.f, 40.f, 8.f });
    player.camera.target = { 9.f, 40.f, 8.f };

    while (!WindowShouldClose())
    {
        player.Update(world);

        BeginDrawing();

        ClearBackground(SKYBLUE);

        BeginMode3D(player.camera);
            DrawGrid(64.f, 1.f);
            world.Draw();
        EndMode3D();

        DrawText("WASD to move, mouse to look", 10, 10, 20, BLACK);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}