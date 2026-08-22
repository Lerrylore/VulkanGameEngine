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
            float2 sampleCoordinates = shadowCoordinates.xy +
                float2(offsetX, offsetY) * texelSize;
            float closestDepth = shadowMap.Sample(sampleCoordinates).r;
            visibility += shadowCoordinates.z - bias <= closestDepth ? 1.0 : 0.2;
        }
    }
    return visibility / 9.0;
}

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
    float3 worldNormal;
    float3 worldPosition;
    float4 worldTangent;
    float4 shadowPosition;
};

Sampler2D texture;
Sampler2D shadowMap;
Sampler2D normalMap;
Sampler2D metallicRoughnessMap;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.worldNormal = normalize(mul((float3x3)ubo.normalMatrix, input.inNormal));
    output.worldPosition = mul(ubo.model, float4(input.inPosition, 1.0)).xyz;
    output.worldTangent = float4(
        normalize(mul((float3x3)ubo.model, input.inTangent.xyz)),
        input.inTangent.w);
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
   float3 geometricNormal = normalize(vertIn.worldNormal);
   float3 worldTangent = normalize(
       vertIn.worldTangent.xyz - geometricNormal * dot(geometricNormal, vertIn.worldTangent.xyz));
   float3 worldBitangent = normalize(cross(geometricNormal, worldTangent)) * vertIn.worldTangent.w;
   float3 tangentNormal = normalMap.Sample(vertIn.fragTexCoord * 1.0).xyz * 2.0 - 1.0;
   float3 worldNormal = normalize(
       worldTangent * tangentNormal.x +
       worldBitangent * tangentNormal.y +
       geometricNormal * tangentNormal.z);
   float3 surfaceToLight = normalize(-ubo.lightDirection.xyz);
   float3 surfaceToCamera = normalize(ubo.cameraPosition.xyz - vertIn.worldPosition);
   float3 halfway = normalize(surfaceToLight + surfaceToCamera);
   float3 shadowCoordinates = vertIn.shadowPosition.xyz / vertIn.shadowPosition.w;
   shadowCoordinates.xy = shadowCoordinates.xy * 0.5 + 0.5;
   float lightFacing = max(dot(geometricNormal, surfaceToLight), 0.0);
   float shadowBias = max(
       ubo.shadowMapParameters.z,
       ubo.shadowMapParameters.w * (1.0 - lightFacing));
   float shadowVisibility = CalculateShadowVisibility(shadowCoordinates, shadowBias);
   float shadowDepth = shadowCoordinates.z >= 0.0 && shadowCoordinates.z <= 1.0
       ? shadowMap.Sample(shadowCoordinates.xy).r
       : 1.0;
   float normalLight = max(dot(worldNormal, surfaceToLight), 0.0);
   float normalView = max(dot(worldNormal, surfaceToCamera), 0.0);
   float4 metallicRoughnessSample = metallicRoughnessMap.Sample(vertIn.fragTexCoord * 1.0);
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
   float3 emissive = ubo.materialEmissive.rgb * ubo.materialEmissive.a;
   float3 albedo = baseColor.rgb * ubo.materialBaseColorAmbient.rgb;

   uint debugView = (uint)ubo.debugView.x;
   if (debugView == 1)
   {
       return float4(EncodeNormal(worldNormal), 1.0);
   }
   if (debugView == 2)
   {
       return float4(EncodeNormal(tangentNormal), 1.0);
   }
   if (debugView == 3)
   {
       return float4(roughness.xxx, 1.0);
   }
   if (debugView == 4)
   {
       return float4(metallic.xxx, 1.0);
   }
   if (debugView == 5)
   {
       return float4(shadowVisibility.xxx, 1.0);
   }
   if (debugView == 6)
   {
       return float4(albedo, baseColor.a);
   }
   if (debugView == 7)
   {
       return float4(EncodeNormal(geometricNormal), 1.0);
   }
   if (debugView == 8)
   {
       return float4(EncodeNormal(worldTangent), 1.0);
   }
   if (debugView == 9)
   {
       return float4(shadowDepth.xxx, 1.0);
   }

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

   if (debugView == 10)
   {
       return float4(diffuse, 1.0);
   }
   if (debugView == 11)
   {
       return float4(specular / (specular + 1.0), 1.0);
   }
   if (debugView == 12)
   {
       return float4(fresnel, 1.0);
   }
   if (debugView == 13)
   {
       return float4(occlusion.xxx, 1.0);
   }
   if (debugView == 14)
   {
       return float4(emissive, 1.0);
   }

   return float4(ambientLighting + directLighting + emissive, baseColor.a);
}
