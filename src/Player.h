#pragma once
#include"raylib.h"
#include"raymath.h"
#include"World.h"


class Player
{
public:
	Vector3 position;
	Vector3 velocity;
	bool onGround;

	Camera3D camera;

	Player(Vector3 startPos);
	void Update(World& world);


private:
	bool CollidesWithWorld(Vector3 pos, World& world);

	float yaw;
	float pitch;

};