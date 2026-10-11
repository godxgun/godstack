#version 450
layout(location=0) in vec2 position;
layout(location=1) in vec2 offset;
layout(location=2) in vec3 color;
layout(location=0) out vec3 vColor;
layout(push_constant) uniform Transform { mat4 transform; } u;
void main() {
  gl_Position = u.transform * vec4(position + offset, 0.0, 1.0);
  vColor = color;
}
