struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
    float3 inNormal;
    float4 inTangent;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 normalMatrix;
    float4x4 shadowViewProjection;
    float4 lightDirection;
    float4 lightColorIntensity;
    float4 materialBaseColorAmbient;
    float4 cameraPosition;
    float4 materialMetallicRoughness;
    float4 materialOcclusion;
    float4 materialEmissive;
    float4 debugView;
    float4 shadowMapParameters;
}
ConstantBuffer<UniformBuffer> ubo;

Sampler2D texture;
Sampler2D shadowMap;
Sampler2D normalMap;
Sampler2D metallicRoughnessMap;

struct VSOutput {
    float4 pos : SV_Position;
    float3 vertexColor;
    float2 fragTexCoord;
    float3 worldNormal;
    float3 worldPosition;
    float4 worldTangent;
};

[shader("vertex")]
VSOutput gbufferVertMain(VSInput input)
{
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.vertexColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.worldNormal = normalize(mul((float3x3)ubo.normalMatrix, input.inNormal));
    output.worldPosition = mul(ubo.model, float4(input.inPosition, 1.0)).xyz;
    output.worldTangent = float4(
        normalize(mul((float3x3)ubo.model, input.inTangent.xyz)),
        input.inTangent.w);
    return output;
}

struct GBufferOutput {
    float4 albedoMetallic : SV_TARGET0;
    float4 normalRoughness : SV_TARGET1;
    float4 worldPosition : SV_TARGET2;
    float4 emissiveOcclusion : SV_TARGET3;
};

[shader("fragment")]
GBufferOutput gbufferFragMain(VSOutput input)
{
    GBufferOutput output;
    float4 baseColor = texture.Sample(input.fragTexCoord);
    float3 geometricNormal = normalize(input.worldNormal);
    float3 worldTangent = normalize(
        input.worldTangent.xyz - geometricNormal * dot(geometricNormal, input.worldTangent.xyz));
    float3 worldBitangent = normalize(cross(geometricNormal, worldTangent)) * input.worldTangent.w;
    float3 tangentNormal = normalMap.Sample(input.fragTexCoord).xyz * 2.0 - 1.0;
    float3 worldNormal = normalize(
        worldTangent * tangentNormal.x +
        worldBitangent * tangentNormal.y +
        geometricNormal * tangentNormal.z);

    float4 metallicRoughnessSample = metallicRoughnessMap.Sample(input.fragTexCoord);
    float metallic = clamp(
        metallicRoughnessSample.b * ubo.materialMetallicRoughness.x,
        0.0,
        1.0);
    float roughness = clamp(
        metallicRoughnessSample.g * ubo.materialMetallicRoughness.y,
        0.04,
        1.0);
    float occlusion = clamp(
        metallicRoughnessSample.r * ubo.materialOcclusion.x,
        0.0,
        1.0);

    output.albedoMetallic = float4(
        baseColor.rgb * input.vertexColor * ubo.materialBaseColorAmbient.rgb,
        metallic);
    output.normalRoughness = float4(worldNormal, roughness);
    output.worldPosition = float4(input.worldPosition, 1.0);
    output.emissiveOcclusion = float4(
        ubo.materialEmissive.rgb * ubo.materialEmissive.a,
        occlusion);
    return output;
}
