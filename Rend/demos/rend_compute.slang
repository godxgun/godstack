struct Particle {
    float2 position;
    float2 velocity;
};

struct PushConstants {
    Particle* particles;
    uint* srcGrid;
    uint* dstGrid;
    uint particleCount;
    uint width;
    uint height;
    float deltaTime;
    uint renderMode; // 0 = particles, 1 = sand 
};

struct VSOutput {
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

[[vk::push_constant]]
PushConstants pc;

uint getIndex(uint x, uint y) {
    return y * pc.width + x;
}

bool isEmpty(uint x, uint y) {
    if (x >= pc.width || y >= pc.height) return false;
    return pc.srcGrid[getIndex(x, y)] == 0;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void particleMain(uint3 threadId : SV_DispatchThreadID) {
    uint index = threadId.x;
    if (index >= pc.particleCount) return;

    Particle p = pc.particles[index];

    p.position += p.velocity * pc.deltaTime;
    p.velocity *= exp(-0.005 * pc.deltaTime);

    pc.particles[index] = p;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void sandMain(uint3 threadId : SV_DispatchThreadID) {
    uint x = threadId.x;
    uint y = threadId.y;

    if (x >= pc.width || y >= pc.height) return;

    uint currentIdx = getIndex(x, y);
    uint cellState = pc.srcGrid[currentIdx];

    if (cellState == 1) { // Sand
        if (y + 1 < pc.height && isEmpty(x, y + 1)) {
            pc.dstGrid[getIndex(x, y + 1)] = 1;
            pc.dstGrid[currentIdx] = 0;
            return;
        }

        bool fallLeftFirst = ((x + y) % 2) == 0;
        int dir1 = fallLeftFirst ? -1 : 1;
        int dir2 = fallLeftFirst ? 1 : -1;

        if (y + 1 < pc.height && isEmpty(x + dir1, y + 1)) {
            pc.dstGrid[getIndex(x + dir1, y + 1)] = 1;
            pc.dstGrid[currentIdx] = 0;
            return;
        } 
        else if (y + 1 < pc.height && isEmpty(x + dir2, y + 1)) {
            pc.dstGrid[getIndex(x + dir2, y + 1)] = 1;
            pc.dstGrid[currentIdx] = 0;
            return;
        }

        pc.dstGrid[currentIdx] = 1;
    }
}

static const float2 QUAD_OFFSETS[6] = {
    float2(-0.5, -0.5), float2( 0.5, -0.5), float2(-0.5,  0.5),
    float2(-0.5,  0.5), float2( 0.5, -0.5), float2( 0.5,  0.5)
};

float3 cosinePalette(float t, float3 a, float3 b, float3 c, float3 d) {
    return a + b * cos(6.28318 * (c * t + d));
}

[shader("vertex")]
VSOutput vertMain(uint vertexID : SV_VertexID) {
    VSOutput output;

    uint elementIdx = vertexID / 6;
    uint cornerIdx  = vertexID % 6;

    float2 worldPos = float2(0.0, 0.0);
    float3 color = float3(0.0, 0.0, 0.0);

    if (pc.renderMode == 0) {
        Particle p = pc.particles[elementIdx];
        worldPos = p.position;

        float speed = length(p.velocity);
        color = cosinePalette(
            speed * 0.8,
            float3(0.5, 0.5, 0.5),
            float3(0.5, 0.5, 0.5),
            float3(1.0, 1.0, 1.0),
            float3(0.0, 0.33, 0.67)
        );
    } 

    else {
        uint x = elementIdx % pc.width;
        uint y = elementIdx / pc.width;

        uint cellState = pc.srcGrid[elementIdx];

        if (cellState == 0) {
            output.position = float4(0, 0, 0, 0);
            output.color = float4(0, 0, 0, 0);
            return output;
        }

        worldPos.x = ((float)x / (float)pc.width) * 2.0 - 1.0;
        worldPos.y = ((float)y / (float)pc.height) * 2.0 - 1.0;

        color = float3(0.94, 0.82, 0.53);
    }

    float2 quadOffset = QUAD_OFFSETS[cornerIdx] * 0.01;
    output.position = float4(worldPos + quadOffset, 0.0, 1.0);
    output.color = float4(color, 1.0);

    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target {
    return input.color;
}
