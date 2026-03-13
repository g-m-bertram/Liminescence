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
        world.Update(player.position);
        player.Update(world);

        BeginDrawing();

        ClearBackground(SKYBLUE);

        BeginMode3D(player.camera);
            DrawGrid(64.f, 1.f);
            world.Draw();
        EndMode3D();

        int cx = GetScreenWidth() / 2;
        int cy = GetScreenHeight() / 2;
        DrawLine(cx - 10, cy, cx + 10, cy, WHITE);
        DrawLine(cx, cy - 10, cx, cy + 10, WHITE);

        DrawText("WASD to move, mouse to look", 10, 10, 20, BLACK);
        DrawText(TextFormat("FPS: %d", GetFPS()), 10, 40, 20, BLACK);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}