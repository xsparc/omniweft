#version 450
// SPDX-License-Identifier: Apache-2.0
layout(location=0) in vec4 clip_position;
layout(location=1) in vec4 color;
layout(location=2) in uint object_id;
layout(location=0) out vec4 face_color;
layout(location=1) flat out uint frame_object_id;
void main() {
    gl_Position = clip_position;
    face_color = color;
    frame_object_id = object_id;
}

