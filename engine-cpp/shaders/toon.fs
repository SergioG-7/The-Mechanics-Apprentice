#version 330

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragPosition;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Hardcodeado para el paso 1; pasará a uniform cuando ShaderManager
// exponga SetValue() en una iteración posterior.
const vec3 lightDir = vec3(-0.4, -1.0, -0.3);
const int  bands    = 4;

out vec4 finalColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    float ndotl = max(dot(normal, normalize(-lightDir)), 0.0);

    // Cuantiza la iluminación continua en bandas discretas -> look cel-shaded.
    float toonFactor = ceil(ndotl * float(bands)) / float(bands);

    vec4 texColor = texture(texture0, fragTexCoord);
    vec3 litColor = texColor.rgb * colDiffuse.rgb * mix(0.35, 1.0, toonFactor);

    finalColor = vec4(litColor, texColor.a * colDiffuse.a);
}
