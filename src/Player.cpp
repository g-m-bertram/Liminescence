#include"Player.h"
#include<initializer_list>
#include<iostream>

// ~~~~~~ PHYSICS CONSTANTS ~~~~~~
const float GRAVITY = -20.f;
const float MOVE_SPEED = 7.f;
const float JUMP_FORCE = 8.f;
const float PLAYER_HEIGHT = 1.8f;
const float PLAYER_WIDTH = 0.6f;
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Player::Player(Vector3 startPos)
{
	position = startPos;
	velocity = { 0, -0.1f, 0 };
	onGround = false;

	camera = {};
	camera.up = { 0.f, 1.f, 0.f };
	camera.fovy = 70.f;
	camera.projection = CAMERA_PERSPECTIVE;

	yaw = 0.f;
	pitch = 0.f;
}

bool Player::CollidesWithWorld(Vector3 pos, World& world)
{
	// check corners of player hitbox
	float half = PLAYER_WIDTH / 2.f;

	for (float dx : {-half, half})
		for (float dz : {-half, half})
			for (float dy : {0.f, PLAYER_HEIGHT * 0.5f, PLAYER_HEIGHT})
			{
				int bx = (int)floor(pos.x + dx);
				int by = (int)floor(pos.y + dy);
				int bz = (int)floor(pos.z + dz);

				if (world.GetBlock(bx, by, bz) != BLOCK_AIR) { return true; }
			}
	return false;
}

void Player::Update(World& world)
{
	float dt = GetFrameTime();

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// ~~~~~ HORIZONTAL MOVEMENT ~~~~~
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	Vector2 input = { 0,0 };
	if (IsKeyDown(KEY_W)) { input.y += 1; }
	if (IsKeyDown(KEY_S)) { input.y -= 1; }
	if (IsKeyDown(KEY_A)) { input.x -= 1; }
	if (IsKeyDown(KEY_D)) { input.x += 1; }

	// get camera forward and right directions
	Vector3 forward = Vector3Subtract(camera.target, camera.position);
	forward.y = 0;
	forward = Vector3Normalize(forward);
	Vector3 right = Vector3CrossProduct(forward, { 0, 1, 0 });

	Vector3 moveDir = Vector3Add(
		Vector3Scale(forward, input.y),
		Vector3Scale(right, input.x)
	);
	if (Vector3Length(moveDir) > 0) { moveDir = Vector3Normalize(moveDir); }

	// try to move horizontally
	Vector3 newPos = position;
	newPos.x += moveDir.x * MOVE_SPEED * dt;
	if (!CollidesWithWorld(newPos, world))
		position.x = newPos.x;
	else
		newPos.x = position.x;
	newPos.z += moveDir.z * MOVE_SPEED * dt;
	if (!CollidesWithWorld(newPos, world))
		position.z = newPos.z;
	else
		newPos.z = position.z;

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// ~~~~~ GRAVITY AND JUMPING ~~~~~
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	velocity.y += GRAVITY * dt;

	if (IsKeyPressed(KEY_SPACE) && onGround)
		velocity.y = JUMP_FORCE;

	// try to move vertically
	float newY = position.y + velocity.y * dt;
	if (!CollidesWithWorld({position.x, newY, position.z}, world))
	{
		position.y = newY;
		onGround = false;
	}
	else
	{
		if (velocity.y < 0)
		{ 
			onGround = true;
			position.y = floorf(position.y); // snap to top of block below
		}
		velocity.y = 0;
	}

	// ~~~~~~~~~~~~~~~~~~
	// ~~~~~ CAMERA ~~~~~
	// ~~~~~~~~~~~~~~~~~~
	// handle mouse look
	Vector2 mouseDelta = GetMouseDelta();
	float sensitivity = 0.003f;

	yaw += -mouseDelta.x * sensitivity;
	pitch += -mouseDelta.y * sensitivity;

	// clamp pitch
	if (pitch > 1.5f) { pitch = 1.5f; }
	if (pitch < -1.5f) { pitch = -1.5f; }

	// calc look direction from yaw and pitch
	Vector3 dir;
	dir.x = cosf(pitch) * sinf(yaw);
	dir.y = sinf(pitch);
	dir.z = cosf(pitch) * cosf(yaw);
		
	camera.position = { position.x, position.y + PLAYER_HEIGHT * 0.9f, position.z };
	camera.target = Vector3Add(camera.position, dir);
}
