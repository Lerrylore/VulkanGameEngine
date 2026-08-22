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

    const float particleLimit = 0.85;

    if (particle.position.x <= -particleLimit || particle.position.x >= particleLimit)
    {
        particle.velocity.x = -particle.velocity.x;
        particle.position.x = clamp(particle.position.x, -particleLimit, particleLimit);
    }
    if (particle.position.y <= -particleLimit || particle.position.y >= particleLimit)
    {
        particle.velocity.y = -particle.velocity.y;
        particle.position.y = clamp(particle.position.y, -particleLimit, particleLimit);
    }

    particlesOut[index] = particle;
}
