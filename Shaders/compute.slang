struct Particle
{
    float2 position;
    float2 velocity;
    float4 color;
};

struct ComputeUniformBuffer
{
    float deltaTime;
};

ConstantBuffer<ComputeUniformBuffer> computeUbo;
StructuredBuffer<Particle> particlesIn;
RWStructuredBuffer<Particle> particlesOut;

[shader("compute")]
[numthreads(256, 1, 1)]
void compMain(uint3 threadId : SV_DispatchThreadID)
{
    uint index = threadId.x;
    Particle particle = particlesIn[index];
    particle.position += particle.velocity * computeUbo.deltaTime;

    if (particle.position.x <= -1.0 || particle.position.x >= 1.0)
    {
        particle.velocity.x = -particle.velocity.x;
        particle.position.x = clamp(particle.position.x, -1.0, 1.0);
    }
    if (particle.position.y <= -1.0 || particle.position.y >= 1.0)
    {
        particle.velocity.y = -particle.velocity.y;
        particle.position.y = clamp(particle.position.y, -1.0, 1.0);
    }

    particlesOut[index] = particle;
}
