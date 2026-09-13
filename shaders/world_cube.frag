#version 450
// SPDX-License-Identifier: Apache-2.0
layout(location=0) in vec4 face_color;
layout(location=1) flat in uint frame_object_id;
layout(location=0) out vec4 out_color;
layout(location=1) out uint out_id;
void main() {
    out_color = face_color;
    out_id = frame_object_id;
}

