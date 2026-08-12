struct ParticleInput
{
    float2 position;
    float4 color;
};

struct ParticleVertexOutput
{
    float4 position : SV_Position;
    float pointSize : SV_PointSize;
    float4 color : COLOR0;
};

struct ParticleFragmentInput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 pointCoord : SV_PointCoord;
};

[shader("vertex")]
ParticleVertexOutput particleVertMain(ParticleInput input)
{
    ParticleVertexOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.pointSize = 8.0;
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 particleFragMain(ParticleFragmentInput input) : SV_Target
{
    float distanceFromCenter = length(input.pointCoord - float2(0.5));
    float alpha = smoothstep(0.5, 0.35, distanceFromCenter);
    return float4(input.color.rgb, input.color.a * alpha);
}
