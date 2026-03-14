#include "Liminescence.h"
#include"raylib.h"
#include"World.h"
#include"Player.h"


int main()
{
    InitWindow(800, 600, "Liminescence");
    SetTargetFPS(60);
    DisableCursor();

    { //scope here to force world and player before opengl context is destroyed by CloseWindow()
        World world;
        Player player({ 8.f, 40.f, 8.f });
        player.camera.target = { 9.f, 40.f, 8.f };

        while (!WindowShouldClose())
        {
            world.Update(player.position);
            player.Update(world);

            Vector3 camDir = Vector3Subtract(player.camera.target, player.camera.position);
            camDir = Vector3Normalize(camDir);

            RaycastResult ray = world.Raycast(player.camera.position, camDir, 8.f);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))            // break block
                world.SetBlock(ray.x, ray.y, ray.z, BLOCK_AIR);
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
                world.SetBlock(ray.nx, ray.ny, ray.nz, BLOCK_GRASS);// place grass

            BeginDrawing();

            ClearBackground(SKYBLUE);

            BeginMode3D(player.camera);
            DrawGrid(64.f, 1.f);
            world.Draw();
            if (ray.hit)
            {
                // highlight selected block
                DrawCubeWires({ ray.x + 0.5f, ray.y + 0.5f, ray.z + 0.5f },
                    1.f, 1.f, 1.f, BLACK);
            }
            EndMode3D();

            // crosshairs
            int cx = GetScreenWidth() / 2;
            int cy = GetScreenHeight() / 2;
            DrawLine(cx - 10, cy, cx + 10, cy, WHITE);
            DrawLine(cx, cy - 10, cx, cy + 10, WHITE);

            // hud info
            DrawText("WASD to move, mouse to look", 10, 10, 20, BLACK);
            DrawText(TextFormat("FPS: %d", GetFPS()), 10, 40, 20, BLACK);

            EndDrawing();
        }
    }
    CloseWindow();
    return 0;
}