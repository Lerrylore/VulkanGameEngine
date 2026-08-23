struct DeferredUniformBuffer {
    float4x4 shadowViewProjection;
    float4 lightDirection;
    float4 lightColorIntensity;
    float4 cameraPosition;
    float4 materialBaseColorAmbient;
    float4 materialMetallicRoughness;
    float4 materialOcclusion;
    float4 materialEmissive;
    float4 debugView;
    float4 shadowMapParameters;
}
ConstantBuffer<DeferredUniformBuffer> ubo;

Sampler2D gbufferAlbedoMetallic;
Sampler2D gbufferNormalRoughness;
Sampler2D gbufferWorldPosition;
Sampler2D gbufferEmissiveOcclusion;
Sampler2D shadowMap;

static const float PI = 3.14159265;

float DistributionGGX(float3 normal, float3 halfway, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float normalHalfway = max(dot(normal, halfway), 0.0);
    float normalHalfwaySquared = normalHalfway * normalHalfway;
    float denominator = normalHalfwaySquared * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / max(PI * denominator * denominator, 0.0001);
}

float GeometrySchlickGGX(float normalDirection, float roughness)
{
    float remappedRoughness = roughness + 1.0;
    float k = remappedRoughness * remappedRoughness / 8.0;
    return normalDirection / (normalDirection * (1.0 - k) + k);
}

float GeometrySmith(float3 normal, float3 viewDirection, float3 lightDirection, float roughness)
{
    return GeometrySchlickGGX(max(dot(normal, viewDirection), 0.0), roughness) *
        GeometrySchlickGGX(max(dot(normal, lightDirection), 0.0), roughness);
}

float3 FresnelSchlick(float cosine, float3 baseReflectivity)
{
    return baseReflectivity + (1.0 - baseReflectivity) * pow(1.0 - cosine, 5.0);
}

float3 EncodeNormal(float3 normal)
{
    return normal * 0.5 + 0.5;
}

float CalculateShadowVisibility(float3 shadowCoordinates, float bias)
{
    if (shadowCoordinates.x < 0.0 || shadowCoordinates.x > 1.0 ||
        shadowCoordinates.y < 0.0 || shadowCoordinates.y > 1.0 ||
        shadowCoordinates.z < 0.0 || shadowCoordinates.z > 1.0)
    {
        return 1.0;
    }

    float visibility = 0.0;
    float2 texelSize = ubo.shadowMapParameters.xy;
    for (int offsetX = -1; offsetX <= 1; ++offsetX)
    {
        for (int offsetY = -1; offsetY <= 1; ++offsetY)
        {
            float2 coordinates = shadowCoordinates.xy + float2(offsetX, offsetY) * texelSize;
            float closestDepth = shadowMap.Sample(coordinates).r;
            visibility += shadowCoordinates.z - bias <= closestDepth ? 1.0 : 0.2;
        }
    }
    return visibility / 9.0;
}

struct FSOutput {
    float4 pos : SV_Position;
    float2 uv;
};

[shader("vertex")]
FSOutput deferredVertMain(uint vertexId : SV_VertexID)
{
    const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0)};
    FSOutput output;
    output.pos = float4(positions[vertexId], 0.0, 1.0);
    // The camera projection already applies Vulkan's Y correction. Keep the
    // fullscreen UV origin aligned with the G-buffer instead of flipping it a
    // second time here.
    output.uv = positions[vertexId] * 0.5 + 0.5;
    return output;
}

[shader("fragment")]
float4 deferredFragMain(FSOutput input) : SV_TARGET
{
    float4 albedoMetallic = gbufferAlbedoMetallic.Sample(input.uv);
    float4 normalRoughness = gbufferNormalRoughness.Sample(input.uv);
    float3 worldPosition = gbufferWorldPosition.Sample(input.uv).xyz;
    float4 emissiveOcclusion = gbufferEmissiveOcclusion.Sample(input.uv);
    float3 worldNormal = normalize(normalRoughness.xyz);
    float roughness = clamp(normalRoughness.w, 0.04, 1.0);
    float metallic = clamp(albedoMetallic.a, 0.0, 1.0);
    float occlusion = clamp(emissiveOcclusion.a, 0.0, 1.0);
    float3 albedo = albedoMetallic.rgb;
    float3 emissive = emissiveOcclusion.rgb;

    float3 surfaceToLight = normalize(-ubo.lightDirection.xyz);
    float3 surfaceToCamera = normalize(ubo.cameraPosition.xyz - worldPosition);
    float3 halfway = normalize(surfaceToLight + surfaceToCamera);
    float4 shadowPosition = mul(ubo.shadowViewProjection, float4(worldPosition, 1.0));
    float3 shadowCoordinates = shadowPosition.xyz / shadowPosition.w;
    shadowCoordinates.xy = shadowCoordinates.xy * 0.5 + 0.5;
    float lightFacing = max(dot(worldNormal, surfaceToLight), 0.0);
    float shadowBias = max(
        ubo.shadowMapParameters.z,
        ubo.shadowMapParameters.w * (1.0 - lightFacing));
    float shadowVisibility = CalculateShadowVisibility(shadowCoordinates, shadowBias);
    float normalLight = max(dot(worldNormal, surfaceToLight), 0.0);
    float normalView = max(dot(worldNormal, surfaceToCamera), 0.0);
    float3 baseReflectivity = lerp(float3(0.04), albedo, metallic);
    float3 fresnel = FresnelSchlick(max(dot(halfway, surfaceToCamera), 0.0), baseReflectivity);
    float normalDistribution = DistributionGGX(worldNormal, halfway, roughness);
    float geometry = GeometrySmith(worldNormal, surfaceToCamera, surfaceToLight, roughness);
    float3 specular = normalDistribution * geometry * fresnel /
        max(4.0 * normalView * normalLight, 0.001);
    float3 diffuseRatio = (1.0 - fresnel) * (1.0 - metallic);
    float3 diffuse = diffuseRatio * albedo / PI;
    float3 radiance = ubo.lightColorIntensity.rgb * ubo.lightColorIntensity.a;
    float3 directLighting = (diffuse + specular) * radiance * normalLight * shadowVisibility;
    float3 ambientLighting = ubo.materialBaseColorAmbient.aaa * diffuseRatio * albedo * occlusion;

    uint debugView = (uint)ubo.debugView.x;
    if (debugView == 1 || debugView == 7) return float4(EncodeNormal(worldNormal), 1.0);
    if (debugView == 3) return float4(roughness.xxx, 1.0);
    if (debugView == 4) return float4(metallic.xxx, 1.0);
    if (debugView == 5) return float4(shadowVisibility.xxx, 1.0);
    if (debugView == 6) return float4(albedo, 1.0);
    if (debugView == 9) return float4(shadowCoordinates.zzz, 1.0);
    if (debugView == 10) return float4(diffuse, 1.0);
    if (debugView == 11) return float4(specular / (specular + 1.0), 1.0);
    if (debugView == 12) return float4(fresnel, 1.0);
    if (debugView == 13) return float4(occlusion.xxx, 1.0);
    if (debugView == 14) return float4(emissive, 1.0);
    return float4(ambientLighting + directLighting + emissive, 1.0);
}
