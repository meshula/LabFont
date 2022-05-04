
@module labimm

@vs vs
uniform vs_params {
    mat4 projectionMatrix;
    mat4 textureMatrix;
};

in vec4 position;
in vec2 texcoord0;
in vec4 color0;
out vec4 uv;
out vec4 color;

void main() {
    gl_Position = projectionMatrix * position;
    uv = textureMatrix * vec4(texcoord0, 0.0, 1.0);
    color = color0;
}

@end

@fs fs

uniform fs_params {
    int texture_slot;
};

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;
uniform sampler2D tex3;
in vec4 uv;
in vec4 color;
out vec4 fragColor;
void main() {
    switch(texture_slot) {
        case 0:
            fragColor = texture(tex0, uv.xy) * color;
            break;
        case 1:
            fragColor = texture(tex1, uv.xy) * color;
            break;
        case 2:
            fragColor = texture(tex2, uv.xy) * color;
            break;
        case 3:
            fragColor = texture(tex3, uv.xy) * color;
            break;
    }
}
@end

@program labimm vs fs

