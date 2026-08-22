struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
    float3 inNormal;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 normalMatrix;
}
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
    float3 worldNormal;
};

Sampler2D texture;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.worldNormal = normalize(mul((float3x3)ubo.normalMatrix, input.inNormal));
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   float3 worldNormal = normalize(vertIn.worldNormal);
   float normalPathValidation = dot(worldNormal, worldNormal);
   return texture.Sample(vertIn.fragTexCoord * 1.0) * lerp(1.0, normalPathValidation, 0.0);
}
