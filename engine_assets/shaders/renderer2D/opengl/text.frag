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
    return vec4(texture(u_Textures[int(i_TexIndex)], i_Vertex.TexCoord));
}

vec2 texSize(){
    return vec2(textureSize(u_Textures[int(i_TexIndex)], 0));
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

void main() {
    vec4 texColor = i_Vertex.Color;
    vec3 msd = textureColor().rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange() * (sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    if (opacity < 0.001) discard;
    o_Color = vec4(texColor.rgb, texColor.a * opacity);
    if (o_Color.a < 0.001) discard;
    o_EntityID = i_EntityID;
}
