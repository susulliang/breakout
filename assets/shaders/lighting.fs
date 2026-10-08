#version 330

/**
 * lighting.fs - 2D dynamic point lighting for the isometric scene (Task 6.2).
 *
 * The scene is first rendered into a RenderTexture2D; that texture is then
 * presented through this shader, which darkens everything by a global ambient
 * level and re-lights it with up to MAX_LIGHTS point lights uploaded from
 * LightingSystem (positions in screen pixels, radius in pixels).
 *
 * The matched C++ side lives in src/LightingSystem.cpp - keep the uniform names
 * and the light budget in sync.
 */

#define MAX_LIGHTS 16

in vec2 fragTexCoord;      // provided by raylib's default vertex shader
in vec4 fragColor;         // provided by raylib's default vertex shader

uniform sampler2D texture0;   // the scene render texture
uniform vec4 colDiffuse;

uniform float ambientLevel;              // 0..1, global darkening of the facility
uniform int   lightCount;                // number of valid entries in the arrays below
uniform vec2  resolution;                // render target size in pixels
uniform vec2  lightPos[MAX_LIGHTS];      // screen pixel position (top-left origin)
uniform vec4  lightColor[MAX_LIGHTS];    // rgb = colour, a = intensity
uniform float lightRadius[MAX_LIGHTS];   // falloff radius in pixels

out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;

    // GLSL y is bottom-up, the light positions are top-down screen pixels.
    vec2 pixel = vec2(gl_FragCoord.x, resolution.y - gl_FragCoord.y);

    // Ambient floor: never fully black, so geometry stays readable even before
    // any light is in range. The clamp is a second line of defence on top of the
    // C++ side, so a low/zero ambientLevel can no longer black the map out.
    float ambient = max(ambientLevel, 0.28);
    vec3 lighting = vec3(ambient * 0.60, ambient * 0.66, ambient * 0.85);

    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        if (i >= lightCount)
        {
            break;
        }

        float radius = max(lightRadius[i], 1.0);
        float distance = length(pixel - lightPos[i]);

        // Smooth quadratic falloff reaching exactly zero at the radius.
        float falloff = 1.0 - clamp(distance / radius, 0.0, 1.0);
        falloff *= falloff;

        lighting += lightColor[i].rgb * (falloff * lightColor[i].a);
    }

    // Soft tonemap keeps the hot cores from clipping to flat white.
    vec3 lit = texel.rgb * lighting;
    lit = lit / (lit + vec3(0.85));
    lit = pow(lit, vec3(0.85));

    finalColor = vec4(lit, 1.0);
}
