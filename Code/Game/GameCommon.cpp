#include "GameCommon.h"
#include <Engine/Math/Vec3.h>
#include <Engine/Math/MathUtils.h>
#include "Engine/Math/Vec2.hpp"
#include <Engine/Core/Vertex_PCU.h>
#include "Engine/Renderer/Renderer.h"

void DebugDrawRing(Vec2 const& center, float radius, float thickness, Rgba8 const& color)
{
	float halfThickness = thickness * 0.5f;
	float innerRadius = radius - halfThickness;
	float outerRadius = radius + halfThickness;

	constexpr int NUM_SIDES = 32;
	constexpr int NUM_VERTS = NUM_SIDES * 6;
	Vertex_PCU verts[NUM_VERTS];
	constexpr float DEGREES_PER_SIDE = 360.f / static_cast<float>(NUM_SIDES);

	for (int sideNum = 0; sideNum < NUM_SIDES; ++sideNum)
	{
		float startDegrees = DEGREES_PER_SIDE * static_cast<float>(sideNum);
		float endDegrees = DEGREES_PER_SIDE * static_cast<float>(sideNum + 1);

		//Compute angle-related terms
		float cosStart = CosDegrees(startDegrees);
		float sinStart = SinDegrees(startDegrees);
		float cosEnd = CosDegrees(endDegrees);
		float sinEnd = SinDegrees(endDegrees);

		//Compute inner and outer positions
		Vec3 innerStartPos = Vec3(center.x + innerRadius * cosStart, center.y + innerRadius * sinStart, 0.f);
		Vec3 outerStartPos = Vec3(center.x + outerRadius * cosStart, center.y + outerRadius * sinStart, 0.f);
		Vec3 innerEndPos = Vec3(center.x + innerRadius * cosEnd, center.y + innerRadius * sinEnd, 0.f);
		Vec3 outerEndPos = Vec3(center.x + outerRadius * cosEnd, center.y + outerRadius * sinEnd, 0.f);

		int vertIndexA = (6 * sideNum) + 0;
		int vertIndexB = (6 * sideNum) + 1;
		int vertIndexC = (6 * sideNum) + 2;
		int vertIndexD = (6 * sideNum) + 3;
		int vertIndexE = (6 * sideNum) + 4;
		int vertIndexF = (6 * sideNum) + 5;


		verts[vertIndexA].m_position = innerEndPos;
		verts[vertIndexB].m_position = innerStartPos;
		verts[vertIndexC].m_position = outerStartPos;
		verts[vertIndexA].m_color = color;
		verts[vertIndexB].m_color = color;
		verts[vertIndexC].m_color = color;

		verts[vertIndexD].m_position = innerEndPos;
		verts[vertIndexE].m_position = outerStartPos;
		verts[vertIndexF].m_position = outerEndPos;
		verts[vertIndexD].m_color = color;
		verts[vertIndexE].m_color = color;
		verts[vertIndexF].m_color = color;

	}

	g_theRenderer->DrawVertexArray(NUM_VERTS, verts);


}

void DebugDrawLine(Vec2 const& start, Vec2 const& end, float thickness, Rgba8 const& color)
{
	Vec2 startEnd = end - start;

	Vec2 h = Vec2(-startEnd.y, startEnd.x); 
	h.Normalize(); 
	h *= thickness * 0.5f; 

	// Calculate the four corner points of the triangles
	Vec2 startLeft = start + h;
	Vec2 endLeft = end + h;
	Vec2 endRight = end - h;
	Vec2 startRight = start - h;

	// Vertices for two triangles
	constexpr int NUM_TRIS = 2;
	constexpr int NUM_VERTS = NUM_TRIS * 3;
	Vertex_PCU verts[NUM_VERTS];

	// First triangle
	verts[0].m_position = Vec3(startLeft.x, startLeft.y, 0.f);
	verts[1].m_position = Vec3(endLeft.x, endLeft.y, 0.f);
	verts[2].m_position = Vec3(endRight.x, endRight.y, 0.f);
	verts[0].m_color = color;
	verts[1].m_color = color;
	verts[2].m_color = color;

	// Second triangle
	verts[3].m_position = Vec3(endRight.x, endRight.y, 0.f);
	verts[4].m_position = Vec3(startRight.x, startRight.y, 0.f);
	verts[5].m_position = Vec3(startLeft.x, startLeft.y, 0.f);
	verts[3].m_color = color;
	verts[4].m_color = color;
	verts[5].m_color = color;

	g_theRenderer->DrawVertexArray(NUM_VERTS, verts);
}


