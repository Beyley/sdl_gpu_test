#version 450

layout(location = 0) in vec2 v_pos;
layout(location = 1) in vec2 v_tex;

layout(location = 0) out vec2 f_tex;

void main() {
    gl_Position = vec4(v_pos, 0, 1);

    f_tex = v_tex;
}