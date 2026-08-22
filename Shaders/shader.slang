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
    float4x4 shadowViewProjection;
    float4 lightDirection;
    float4 lightColorIntensity;
    float4 materialBaseColorAmbient;
    float4 cameraPosition;
    float4 materialMetallicRoughness;
}
ConstantBuffer<UniformBuffer> ubo;

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
    float normalView = max(dot(normal, viewDirection), 0.0);
    float normalLight = max(dot(normal, lightDirection), 0.0);
    return GeometrySchlickGGX(normalView, roughness) *
        GeometrySchlickGGX(normalLight, roughness);
}

float3 FresnelSchlick(float cosine, float3 baseReflectivity)
{
    return baseReflectivity +
        (1.0 - baseReflectivity) * pow(1.0 - cosine, 5.0);
}

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
    float3 worldNormal;
    float3 worldPosition;
    float4 shadowPosition;
};

Sampler2D texture;
Sampler2D shadowMap;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.worldNormal = normalize(mul((float3x3)ubo.normalMatrix, input.inNormal));
    output.worldPosition = mul(ubo.model, float4(input.inPosition, 1.0)).xyz;
    output.shadowPosition = mul(ubo.shadowViewProjection, mul(ubo.model, float4(input.inPosition, 1.0)));
    return output;
}

[shader("vertex")]
float4 shadowVertMain(VSInput input) : SV_Position
{
    return mul(ubo.shadowViewProjection, mul(ubo.model, float4(input.inPosition, 1.0)));
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   float4 baseColor = texture.Sample(vertIn.fragTexCoord * 1.0);
   float3 worldNormal = normalize(vertIn.worldNormal);
   float3 surfaceToLight = normalize(-ubo.lightDirection.xyz);
   float3 surfaceToCamera = normalize(ubo.cameraPosition.xyz - vertIn.worldPosition);
   float3 halfway = normalize(surfaceToLight + surfaceToCamera);
   float3 shadowCoordinates = vertIn.shadowPosition.xyz / vertIn.shadowPosition.w;
   shadowCoordinates.xy = shadowCoordinates.xy * 0.5 + 0.5;
   float shadowVisibility = 1.0;
   if (shadowCoordinates.x >= 0.0 && shadowCoordinates.x <= 1.0 &&
       shadowCoordinates.y >= 0.0 && shadowCoordinates.y <= 1.0 &&
       shadowCoordinates.z >= 0.0 && shadowCoordinates.z <= 1.0)
   {
       float closestDepth = shadowMap.Sample(shadowCoordinates.xy).r;
       shadowVisibility = shadowCoordinates.z - 0.005 <= closestDepth ? 1.0 : 0.2;
   }
   float normalLight = max(dot(worldNormal, surfaceToLight), 0.0);
   float normalView = max(dot(worldNormal, surfaceToCamera), 0.0);
   float metallic = clamp(ubo.materialMetallicRoughness.x, 0.0, 1.0);
   float roughness = clamp(ubo.materialMetallicRoughness.y, 0.04, 1.0);
   float3 albedo = baseColor.rgb * ubo.materialBaseColorAmbient.rgb;
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
   float3 ambientLighting = ubo.materialBaseColorAmbient.aaa * diffuseRatio * albedo;
   return float4(ambientLighting + directLighting, baseColor.a);
}
