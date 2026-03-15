#version 330

in vec3 fragPosition;
in vec3 fragNormal;
in vec4 fragColor;

uniform vec3 cameraPosition;
uniform vec4 fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform float time;

out vec4 finalColor;

void main()
{
	vec4 color = fragColor;
	color.a = 0.75;

	// lighting
	vec3 lightDir = normalize(vec3(0.6, 1.0, 0.4));
	float diff = max(dot(fragNormal, lightDir), 0.0);
	float ambient = 0.5;
	color.rgb *= (ambient + diff * 0.5);

	// specular highlight on water surface
	vec3 viewDir = normalize(cameraPosition - fragPosition);
	vec3 reflectDir = reflect(-lightDir, fragNormal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
	color.rgb += vec3(0.3) * spec;

	// fog
	float dist = length(cameraPosition - fragPosition);
	float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
	color = mix(color, fogColor, fogFactor);

	finalColor = color;
}