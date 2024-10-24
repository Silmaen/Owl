#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

struct VertexOutput{
    vec4 Color;
    vec2 TexCoord;
};

layout (location = 0) in VertexOutput i_Vertex;
layout (location = 2) in flat float i_TexIndex;
layout (location = 3) in flat int i_EntityID;

layout (binding = 0) uniform sampler2D u_Textures[32];

vec4 textureColor(){
    vec4 tex;
    switch (int(i_TexIndex)) {
        case 0: tex = vec4(texture(u_Textures[0], i_Vertex.TexCoord)); break;
        case 1: tex = vec4(texture(u_Textures[1], i_Vertex.TexCoord)); break;
        case 2: tex = vec4(texture(u_Textures[2], i_Vertex.TexCoord)); break;
        case 3: tex = vec4(texture(u_Textures[3], i_Vertex.TexCoord)); break;
        case 4: tex = vec4(texture(u_Textures[4], i_Vertex.TexCoord)); break;
        case 5: tex = vec4(texture(u_Textures[5], i_Vertex.TexCoord)); break;
        case 6: tex = vec4(texture(u_Textures[6], i_Vertex.TexCoord)); break;
        case 7: tex = vec4(texture(u_Textures[7], i_Vertex.TexCoord)); break;
        case 8: tex = vec4(texture(u_Textures[8], i_Vertex.TexCoord)); break;
        case 9: tex = vec4(texture(u_Textures[9], i_Vertex.TexCoord)); break;
        case 10: tex = vec4(texture(u_Textures[10], i_Vertex.TexCoord)); break;
        case 11: tex = vec4(texture(u_Textures[11], i_Vertex.TexCoord)); break;
        case 12: tex = vec4(texture(u_Textures[12], i_Vertex.TexCoord)); break;
        case 13: tex = vec4(texture(u_Textures[13], i_Vertex.TexCoord)); break;
        case 14: tex = vec4(texture(u_Textures[14], i_Vertex.TexCoord)); break;
        case 15: tex = vec4(texture(u_Textures[15], i_Vertex.TexCoord)); break;
        case 16: tex = vec4(texture(u_Textures[16], i_Vertex.TexCoord)); break;
        case 17: tex = vec4(texture(u_Textures[17], i_Vertex.TexCoord)); break;
        case 18: tex = vec4(texture(u_Textures[18], i_Vertex.TexCoord)); break;
        case 19: tex = vec4(texture(u_Textures[19], i_Vertex.TexCoord)); break;
        case 20: tex = vec4(texture(u_Textures[20], i_Vertex.TexCoord)); break;
        case 21: tex = vec4(texture(u_Textures[21], i_Vertex.TexCoord)); break;
        case 22: tex = vec4(texture(u_Textures[22], i_Vertex.TexCoord)); break;
        case 23: tex = vec4(texture(u_Textures[23], i_Vertex.TexCoord)); break;
        case 24: tex = vec4(texture(u_Textures[24], i_Vertex.TexCoord)); break;
        case 25: tex = vec4(texture(u_Textures[25], i_Vertex.TexCoord)); break;
        case 26: tex = vec4(texture(u_Textures[26], i_Vertex.TexCoord)); break;
        case 27: tex = vec4(texture(u_Textures[27], i_Vertex.TexCoord)); break;
        case 28: tex = vec4(texture(u_Textures[28], i_Vertex.TexCoord)); break;
        case 29: tex = vec4(texture(u_Textures[29], i_Vertex.TexCoord)); break;
        case 30: tex = vec4(texture(u_Textures[30], i_Vertex.TexCoord)); break;
        case 31: tex = vec4(texture(u_Textures[31], i_Vertex.TexCoord)); break;
    }
    return tex;
}
vec2 texSize(){
    vec2 tex;
    switch (int(i_TexIndex)) {
        case 0: tex = vec2(textureSize(u_Textures[0], 0)); break;
        case 1: tex = vec2(textureSize(u_Textures[1], 0)); break;
        case 2: tex = vec2(textureSize(u_Textures[2], 0)); break;
        case 3: tex = vec2(textureSize(u_Textures[3], 0)); break;
        case 4: tex = vec2(textureSize(u_Textures[4], 0)); break;
        case 5: tex = vec2(textureSize(u_Textures[5], 0)); break;
        case 6: tex = vec2(textureSize(u_Textures[6], 0)); break;
        case 7: tex = vec2(textureSize(u_Textures[7], 0)); break;
        case 8: tex = vec2(textureSize(u_Textures[8], 0)); break;
        case 9: tex = vec2(textureSize(u_Textures[9], 0)); break;
        case 10: tex = vec2(textureSize(u_Textures[10], 0)); break;
        case 11: tex = vec2(textureSize(u_Textures[11], 0)); break;
        case 12: tex = vec2(textureSize(u_Textures[12], 0)); break;
        case 13: tex = vec2(textureSize(u_Textures[13], 0)); break;
        case 14: tex = vec2(textureSize(u_Textures[14], 0)); break;
        case 15: tex = vec2(textureSize(u_Textures[15], 0)); break;
        case 16: tex = vec2(textureSize(u_Textures[16], 0)); break;
        case 17: tex = vec2(textureSize(u_Textures[17], 0)); break;
        case 18: tex = vec2(textureSize(u_Textures[18], 0)); break;
        case 19: tex = vec2(textureSize(u_Textures[19], 0)); break;
        case 20: tex = vec2(textureSize(u_Textures[20], 0)); break;
        case 21: tex = vec2(textureSize(u_Textures[21], 0)); break;
        case 22: tex = vec2(textureSize(u_Textures[22], 0)); break;
        case 23: tex = vec2(textureSize(u_Textures[23], 0)); break;
        case 24: tex = vec2(textureSize(u_Textures[24], 0)); break;
        case 25: tex = vec2(textureSize(u_Textures[25], 0)); break;
        case 26: tex = vec2(textureSize(u_Textures[26], 0)); break;
        case 27: tex = vec2(textureSize(u_Textures[27], 0)); break;
        case 28: tex = vec2(textureSize(u_Textures[28], 0)); break;
        case 29: tex = vec2(textureSize(u_Textures[29], 0)); break;
        case 30: tex = vec2(textureSize(u_Textures[30], 0)); break;
        case 31: tex = vec2(textureSize(u_Textures[31], 0)); break;
    }
    return tex;
}

float screenPxRange() {
    const float pxRange = 2.0;// set to distance field's pixel range
    vec2 unitRange = vec2(pxRange)/vec2(texSize());
    vec2 screenTexSize = vec2(1.0)/fwidth(i_Vertex.TexCoord);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec4 texColor = i_Vertex.Color;
    vec3 msd = textureColor().rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange()*(sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    if (opacity == 0.0) discard;
    vec4 bgColor = vec4(0.0);
    o_Color = mix(bgColor, i_Vertex.Color, opacity);
    if (o_Color.a == 0.0)
    discard;

    o_EntityID = i_EntityID;
}
