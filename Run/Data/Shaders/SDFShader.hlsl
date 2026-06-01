cbuffer CameraConstants : register(b2)
{
	float4x4 WorldToCameraTransform;
	float4x4 CameraToRenderTransform;
	float4x4 RenderToClipTransform;
};
// -----------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
	float4x4 ModelToWorldTransform;
	float4 ModelColor;
};
// -----------------------------------------------------------------------------
cbuffer SDFShapes : register(b4)
{
	float4 numCircles;
	float4 circleData[32];

    float4 numBoxes;
    float4 boxData[32];

	float4 numOBBs;
    float4 obbData[64];

	float4 numLineSegments;
	float4 lineSegmentData[32];

	float4 numTriangles;
	float4 triangleData[64];

	float4 numCapsules;
	float4 capsuleData[64];

	float4 numPlanes;
	float4 planeData[32];

	float4 renderParams;
	float4 surfaceParams;
};
// -----------------------------------------------------------------------------
cbuffer SDFGlobals : register(b5)
{
    float4 moonParams;
};
// -----------------------------------------------------------------------------
Texture2D diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);
// -----------------------------------------------------------------------------
struct vs_input_t
{
	float3 modelSpacePosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};
// -----------------------------------------------------------------------------
struct v2p_t
{
	float4 clipSpacePosition : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};
// -----------------------------------------------------------------------------
float SDFDisc(float2 referencePoint, float2 center, float radius)
{
    return length(referencePoint - center) - radius;
}
// -----------------------------------------------------------------------------
float SDFBox(float2 referencePoint, float2 center, float2 halfDims)
{
    float2 dist = abs(referencePoint - center) - halfDims;
    float outside = length(max(dist, float2(0.0f, 0.0f)));
    float inside = min(max(dist.x, dist.y), 0.0f);
    return outside + inside;
}
// -----------------------------------------------------------------------------
float SDFOrientedBox(float2 referencePoint, float2 center, float2 halfDims, float2 right, float2 up)
{
    float2 displacement = referencePoint - center;

    float2 local;
    local.x = dot(displacement, right);
    local.y = dot(displacement, up);

    float2 dist = abs(local) - halfDims;

    float outside = length(max(dist, float2(0.f,0.f)));
    float inside = min(max(dist.x, dist.y), 0.0f);
    return outside + inside;
}
// -----------------------------------------------------------------------------
float SDFLineSegment(float2 referencePoint, float2 start, float2 end)
{
	float2 pointToStartDisp = referencePoint - start;
	float2 endToStartDisp = end - start;

	float h = clamp(dot(pointToStartDisp, endToStartDisp) / dot(endToStartDisp, endToStartDisp), 0.f, 1.f);
	return length(pointToStartDisp - endToStartDisp * h);
}
// -----------------------------------------------------------------------------
float SDFTriangle(float2 referencePoint, float2 a, float2 b, float2 c)
{
    float2 ba = b - a; 
	float2 pa = referencePoint - a;
    float2 cb = c - b; 
	float2 pb = referencePoint - b;
    float2 ac = a - c; 
	float2 pc = referencePoint - c;

    float d1 = dot(ba * clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0) - pa, ba * clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0) - pa);
    float d2 = dot(cb * clamp(dot(pb, cb) / dot(cb, cb), 0.0, 1.0) - pb, cb * clamp(dot(pb, cb) / dot(cb, cb), 0.0, 1.0) - pb);
    float d3 = dot(ac * clamp(dot(pc, ac) / dot(ac, ac), 0.0, 1.0) - pc, ac * clamp(dot(pc, ac) / dot(ac, ac), 0.0, 1.0) - pc);

    float dist = sqrt(min(min(d1, d2), d3));
    float s = sign(dot(pa, float2(-ba.y, ba.x))) + sign(dot(pb, float2(-cb.y, cb.x))) + sign(dot(pc, float2(-ac.y, ac.x)));

	if (s < 2.f)
	{
		return dist;
	}
	else
	{
		return -dist;
	}
}
// -----------------------------------------------------------------------------
float SDFCapsule(float2 referencePoint, float2 start, float2 end, float radius)
{
    return SDFLineSegment(referencePoint, start, end) - radius;
}
// -----------------------------------------------------------------------------
float SDFPlane(float2 referencePoint, float2 planeNormal, float distance)
{
    return dot(referencePoint, planeNormal) - distance;
}
// -----------------------------------------------------------------------------
float SDFMoon(float2 referencePoint, float2 center, float r1, float r2, float t)
{
    float mainMoon = length(referencePoint - center) - r1;

    float range = r1 + r2;
	float2 offset = float2(sin(t), 0.0f) * range;
    float orbitingCutter = length(referencePoint - (center + offset)) - r2;

    return max(mainMoon, -orbitingCutter);
}
// -----------------------------------------------------------------------------
float SmoothMin(float a, float b, float k)
{
    float h = max(k - abs(a - b), 0.0f) / k;
    return min(a, b) - h * h * k * 0.25f;
}
// -----------------------------------------------------------------------------
float Combine(float a, float b, float k, int mode)
{
    if (mode == 0)
    {
        return min(a, b);
    }
    else if (mode == 1)
    {
        return SmoothMin(a, b, k);
    }
	else
	{
		return max(a, b);
	}
}
// -----------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
	float4 modelSpacePosition = float4(input.modelSpacePosition, 1.0f);
	float4 worldSpacePosition = mul(ModelToWorldTransform, modelSpacePosition);
	float4 cameraSpacePosition = mul(WorldToCameraTransform, worldSpacePosition);
	float4 renderSpacePosition = mul(CameraToRenderTransform, cameraSpacePosition);
	float4 clipSpacePosition = mul(RenderToClipTransform, renderSpacePosition);

	v2p_t v2p;
	v2p.clipSpacePosition = clipSpacePosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
	return v2p;
}
// -----------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    float2 fragPos = input.uv * float2(1600.f, 800.f);

	float k           = renderParams.x;
	int   renderMode  = (int)renderParams.y;
	int   blendMode   = (int)renderParams.z;
	float aaWidth     = renderParams.w;
	int   surfaceMode = (int)surfaceParams.x;
	float param       = surfaceParams.y;

    float dist;
	if (blendMode == 2)
	{
		dist = -1e9;
	}
	else
	{
		dist = 1e9;
	}

	// Disc
	for (int circleIndex = 0; circleIndex < numCircles.x; ++circleIndex)
	{
		float2 center = circleData[circleIndex].xy;
		float radius = circleData[circleIndex].z;

		float dCircle = SDFDisc(fragPos, center, radius);
		dist = (circleIndex == 0) ? dCircle : Combine(dist, dCircle, k, blendMode);
	}

	// AABB
	for (int boxIndex = 0; boxIndex < numBoxes.x; ++boxIndex)
	{
		float2 center = boxData[boxIndex].xy;
		float2 halfDims = boxData[boxIndex].zw;

		float dBox = SDFBox(fragPos, center, halfDims);
		dist = Combine(dist, dBox, k, blendMode);
	}

	// OBB
	for (int orientedBoxIndex = 0; orientedBoxIndex < numOBBs.x; ++orientedBoxIndex)
	{
		float2 center   = obbData[orientedBoxIndex * 2 + 0].xy;
		float2 halfDims = obbData[orientedBoxIndex * 2 + 0].zw;
		float2 right = obbData[orientedBoxIndex * 2 + 1].xy;
		float2 up    = obbData[orientedBoxIndex * 2 + 1].zw;

		float dOrientedBox = SDFOrientedBox(fragPos, center, halfDims, right, up);
		dist = Combine(dist, dOrientedBox, k, blendMode);
	}

	// Line Segment
	for (int segmentIndex = 0; segmentIndex < numLineSegments.x; ++segmentIndex)
	{
		float2 segmentStart = lineSegmentData[segmentIndex].xy;
		float2 segmentEnd = lineSegmentData[segmentIndex].zw;

		float dSegment = SDFLineSegment(fragPos, segmentStart, segmentEnd);
		dist = Combine(dist, dSegment, k, blendMode);
	}

	// Triangles
	for (int triIndex = 0; triIndex < numTriangles.x; ++triIndex)
	{
		float2 a = triangleData[triIndex * 2 + 0].xy;
		float2 b = triangleData[triIndex * 2 + 0].zw;
		float2 c = triangleData[triIndex * 2 + 1].xy;

		float dTriangle = SDFTriangle(fragPos, a, b, c);
		dist = Combine(dist, dTriangle, k, blendMode);
	}

	// Capsules
	for (int capsuleIndex = 0; capsuleIndex < numCapsules.x; ++capsuleIndex)
	{
		float2 start = capsuleData[capsuleIndex * 2 + 0].xy;
		float2 end = capsuleData[capsuleIndex * 2 + 0].zw;
		float  radius = capsuleData[capsuleIndex * 2 + 1].x;

		float dCapsule = SDFCapsule(fragPos, start, end, radius);
		dist = Combine(dist, dCapsule, k, blendMode);
	}

	// Planes
	for (int planeIndex = 0; planeIndex < numPlanes.x; ++planeIndex)
	{
		float2 normal = planeData[planeIndex].xy;
		float  pDistance = planeData[planeIndex].z;

		float dPlane = SDFPlane(fragPos, normal, pDistance);
		dist = Combine(dist, dPlane, k, blendMode);
	}

	// Moon
	if (moonParams.w > 0.5f)
	{
		float r1 = moonParams.x;
		float r2 = moonParams.y;
		float t  = moonParams.z;

		float dMoon = SDFMoon(fragPos, float2(800, 400), r1, r2, t);
		dist = Combine(dist, dMoon, k, blendMode);
	}

	// Surface mode post process
	if (surfaceMode == 1)
	{
		dist -= param;
	}
	else if (surfaceMode == 2)
	{
		dist = abs(dist) - param;
	}

	float4 insideColor = float4(0,0,1,1);
	float4 outsideColor = float4(1,0,0,1);

	float4 color;

	// Solids
	if (renderMode == 0)
	{
		float inside = step(dist, 0.0f);
		color = lerp(outsideColor, insideColor, inside);
	}
	// Bands
	else if (renderMode == 1)
	{
		float bandSpacing = 20.0f;
		float d = dist / bandSpacing;
		float signMask = (dist < 0.0f) ? 1.0f : 0.0f;

		float3 insideColor  = float3(0.1f, 0.2f, 1.0f);
		float3 outsideColor = float3(1.0f, 0.2f, 0.1f);

		float3 baseColor = lerp(outsideColor, insideColor, signMask);

		float f = abs(d);
		float wave = 0.5f + 0.5f * sin(f * 6.283185f);
		float falloff = exp(-f * 0.8f);
		float bandIntensity = wave * falloff;

		float3 finalColor = baseColor * (0.4f + 0.6f * bandIntensity);
		color = float4(finalColor, 1);
	}
	// Anti-aliasing
	else if (renderMode == 2)
	{
		float smoothing = smoothstep(-aaWidth, aaWidth, dist);
		float3 col = lerp(insideColor.rgb, outsideColor.rgb, smoothing);
		color = float4(col, 1);
	}
	// Glow
	else if (renderMode == 3)
	{
		float glowRadius = param;
		float intensity = exp(-abs(dist) / glowRadius);

		float3 glowColor = float3(0.2f, 0.6f, 1.0f);
		color = float4(glowColor * intensity, 1.0f);
	}
	else
	{
		float gradientT = saturate(dist / 50.0f);
		color = lerp(insideColor, outsideColor, gradientT);
	}

    return color;
}
// -----------------------------------------------------------------------------