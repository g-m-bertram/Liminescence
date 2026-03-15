#version 330

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform float time;
uniform vec2 chunkOffset;

out vec3 fragPosition;
out vec3 fragNormal;
out vec4 fragColor;

void main()
{
	vec3 pos = vertexPosition;

	// use worldspace coords for wave phase only
	float worldX = pos.x + chunkOffset.x;
	float worldZ = pos.z + chunkOffset.y;
	pos.y += sin(worldX * 0.5 + time) * 0.04 + sin(worldZ * 0.5 + time * 0.7) * 0.04;
	
	fragPosition = vec3(matModel * vec4(pos, 1.0));
	fragNormal = vertexNormal;
	fragColor = vertexColor;
	gl_Position = mvp * vec4(pos, 1.0);
}