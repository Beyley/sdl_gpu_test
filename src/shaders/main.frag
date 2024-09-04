#version 450

layout(location = 0) in vec2 f_tex;

layout(location = 0) out vec4 col;

layout(set = 2, binding = 0) uniform sampler2D _texture;

void main() {
    col = texture(_texture, f_tex);
}