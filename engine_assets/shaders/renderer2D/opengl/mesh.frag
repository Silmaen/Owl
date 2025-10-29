#version 450 core

layout (location = 0) out vec4 o_Color;
layout (location = 1) out int o_EntityID;

struct VertexOutput {
    vec4 Color;
    vec2 TexCoord;
    vec3 Normal;
};

layout (location = 0) in VertexOutput i_Vertex;
layout (location = 3) in flat float i_TexIndex;
layout (location = 4) in flat int i_EntityID;

layout (binding = 0) uniform sampler2D u_Textures[32];

void main() {
    vec4 texColor = i_Vertex.Color;
    switch (int(i_TexIndex)) {
        case 0: texColor *= texture(u_Textures[0], i_Vertex.TexCoord); break;
        case 1: texColor *= texture(u_Textures[1], i_Vertex.TexCoord); break;
        case 2: texColor *= texture(u_Textures[2], i_Vertex.TexCoord); break;
        case 3: texColor *= texture(u_Textures[3], i_Vertex.TexCoord); break;
        case 4: texColor *= texture(u_Textures[4], i_Vertex.TexCoord); break;
        case 5: texColor *= texture(u_Textures[5], i_Vertex.TexCoord); break;
        case 6: texColor *= texture(u_Textures[6], i_Vertex.TexCoord); break;
        case 7: texColor *= texture(u_Textures[7], i_Vertex.TexCoord); break;
        case 8: texColor *= texture(u_Textures[8], i_Vertex.TexCoord); break;
        case 9: texColor *= texture(u_Textures[9], i_Vertex.TexCoord); break;
        case 10: texColor *= texture(u_Textures[10], i_Vertex.TexCoord); break;
        case 11: texColor *= texture(u_Textures[11], i_Vertex.TexCoord); break;
        case 12: texColor *= texture(u_Textures[12], i_Vertex.TexCoord); break;
        case 13: texColor *= texture(u_Textures[13], i_Vertex.TexCoord); break;
        case 14: texColor *= texture(u_Textures[14], i_Vertex.TexCoord); break;
        case 15: texColor *= texture(u_Textures[15], i_Vertex.TexCoord); break;
        case 16: texColor *= texture(u_Textures[16], i_Vertex.TexCoord); break;
        case 17: texColor *= texture(u_Textures[17], i_Vertex.TexCoord); break;
        case 18: texColor *= texture(u_Textures[18], i_Vertex.TexCoord); break;
        case 19: texColor *= texture(u_Textures[19], i_Vertex.TexCoord); break;
        case 20: texColor *= texture(u_Textures[20], i_Vertex.TexCoord); break;
        case 21: texColor *= texture(u_Textures[21], i_Vertex.TexCoord); break;
        case 22: texColor *= texture(u_Textures[22], i_Vertex.TexCoord); break;
        case 23: texColor *= texture(u_Textures[23], i_Vertex.TexCoord); break;
        case 24: texColor *= texture(u_Textures[24], i_Vertex.TexCoord); break;
        case 25: texColor *= texture(u_Textures[25], i_Vertex.TexCoord); break;
        case 26: texColor *= texture(u_Textures[26], i_Vertex.TexCoord); break;
        case 27: texColor *= texture(u_Textures[27], i_Vertex.TexCoord); break;
        case 28: texColor *= texture(u_Textures[28], i_Vertex.TexCoord); break;
        case 29: texColor *= texture(u_Textures[29], i_Vertex.TexCoord); break;
        case 30: texColor *= texture(u_Textures[30], i_Vertex.TexCoord); break;
        case 31: texColor *= texture(u_Textures[31], i_Vertex.TexCoord); break;
    }

    // Simple ambient lighting using normal
    float ambient = 0.6;
    float diffuse = 0.4 * max(dot(normalize(i_Vertex.Normal), vec3(0.0, 0.0, 1.0)), 0.0);
    float lighting = ambient + diffuse;

    o_Color = texColor * vec4(vec3(lighting), 1.0);
    o_EntityID = i_EntityID;
}
