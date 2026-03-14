#version 330

in vec3 fragPosition;
in vec3 fragNormal;
in vec4 fragColor;

uniform vec3 cameraPosition;
uniform vec4 fogColor;
uniform float fogStart;
uniform float fogEnd;

out vec4 finalColor;

void main()
{
	// base color from vertex color
	vec4 color = fragColor;

	// simple directional lighting
	vec3 lightDir = normalize(vec3(0.6, 1.0, 0.4));
	float diff = max(dot(fragNormal, lightDir), 0.0);
	float ambient = 0.4;
	color.rgb *= (ambient + diff * 0.6);

	// fog
	float dist = length(cameraPosition - fragPosition);
	float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
	color = mix(color, fogColor, fogFactor);

	finalColor = color;
}