#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D tex_sampler;

/* Pixel filtering algorithm */
vec2 filterer( vec2 uv, ivec2 size ) {
    vec2 pixel = uv * size;

    vec2 seam = floor(pixel + 0.5);
    pixel = seam + clamp((pixel - seam) / fwidth(pixel), -0.5, 0.5);

    return pixel / size;
}

void main() {
    ivec2 texture_size = textureSize(tex_sampler, 0);

    color = texture(tex_sampler, filterer(uv, texture_size));
}
