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
    float4 lightDirection;
    float4 lightColorIntensity;
    float4 materialBaseColorAmbient;
    float4 cameraPosition;
    float4 materialSpecularShininess;
}
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
    float3 worldNormal;
    float3 worldPosition;
};

Sampler2D texture;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.worldNormal = normalize(mul((float3x3)ubo.normalMatrix, input.inNormal));
    output.worldPosition = mul(ubo.model, float4(input.inPosition, 1.0)).xyz;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   float4 baseColor = texture.Sample(vertIn.fragTexCoord * 1.0);
   float3 worldNormal = normalize(vertIn.worldNormal);
   float3 surfaceToLight = normalize(-ubo.lightDirection.xyz);
   float3 surfaceToCamera = normalize(ubo.cameraPosition.xyz - vertIn.worldPosition);
   float diffuse = max(dot(worldNormal, surfaceToLight), 0.0);
   float3 halfway = normalize(surfaceToLight + surfaceToCamera);
   float specularFactor = pow(
       max(dot(worldNormal, halfway), 0.0),
       ubo.materialSpecularShininess.a) * step(0.0, diffuse);
   float3 lighting = ubo.materialBaseColorAmbient.aaa +
       diffuse * ubo.lightColorIntensity.rgb * ubo.lightColorIntensity.a;
   float3 materialColor = baseColor.rgb * ubo.materialBaseColorAmbient.rgb;
   float3 specular = ubo.materialSpecularShininess.rgb * specularFactor *
       ubo.lightColorIntensity.rgb * ubo.lightColorIntensity.a;
   return float4(materialColor * lighting + specular, baseColor.a);
}
