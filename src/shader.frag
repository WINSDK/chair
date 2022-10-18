#version 450

layout(location = 0) in vec2 frag_text_coord;
layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler2D text_sampler;

void main() {
    out_color = texture(text_sampler, frag_text_coord);
}
