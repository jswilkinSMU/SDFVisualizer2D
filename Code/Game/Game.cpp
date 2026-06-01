#include "Game/Game.h"
#include "Game/GameCommon.h"
#include "Game/App.h"

#include "Engine/Input/InputSystem.h"
#include "Engine/Window/Window.hpp"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/Rgba8.h"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/VertexUtils.h"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Math/MathUtils.h"
#include "Engine/Math/Plane2.hpp"

Game::Game(App* owner)
	: m_app(owner)
{
	m_sdfShader = g_theRenderer->CreateOrGetShader("Data/Shaders/SDFShader");
	m_font = g_theRenderer->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");
	m_constantBuffer = g_theRenderer->CreateConstantBuffer(sizeof(SDFShapes));
	m_globalsCB = g_theRenderer->CreateConstantBuffer(sizeof(SDFGlobals));
}

Game::~Game()
{
}

void Game::StartUp()
{
	m_gameClock.Reset();
}

void Game::Update()
{
	double deltaSeconds = m_gameClock.GetDeltaSeconds();
	m_smoothK = GetClamped(m_smoothK, 0.1f, 250.f);

	m_moonTime += static_cast<float>(deltaSeconds);

	AdjustForPauseAndTimeDistortion(static_cast<float>(deltaSeconds));
	UpdateMouseDrag();
	KeyInputPresses(static_cast<float>(deltaSeconds));
	UpdateCameras();
}

void Game::Render() const
{
	g_theRenderer->BeginCamera(m_screenCamera);

	// SDF Fullscreen
	// -----------------------------------------------------------------------------
	SetGlobalsConstants();
	SetSDFShapesConstants();
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(m_sdfShader);

	std::vector<Vertex_PCU> verts;
	AddVertsForAABB2D(verts, AABB2(0.0f, 0.0f, SCREEN_SIZE_X, SCREEN_SIZE_Y), Rgba8::WHITE);
	g_theRenderer->DrawVertexArray(verts);
	// -----------------------------------------------------------------------------

	g_theRenderer->BindShader(nullptr);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);

	// Drag overlay
	// -----------------------------------------------------------------------------
	if (m_isCreatingShape && m_currentShapeMode == ShapeMode::AABB)
	{
		std::vector<Vertex_PCU> overlayVerts;

		Vec2 mins = GetMin(m_dragStart, m_dragEnd);
		Vec2 maxs = GetMax(m_dragStart, m_dragEnd);
		AABB2 preview(mins, maxs);

		AddVertsForAABB2D(overlayVerts, preview, Rgba8(255, 255, 255, 80));
		g_theRenderer->DrawVertexArray(overlayVerts);
	}
	else if (m_isCreatingShape && m_currentShapeMode == ShapeMode::CIRCLE)
	{
		std::vector<Vertex_PCU> overlayVerts;

		float radius = (m_dragEnd - m_dragStart).GetLength();
		AddVertsForDisc2D(overlayVerts, m_dragStart, radius, Rgba8(255, 255, 255, 80));

		g_theRenderer->DrawVertexArray(overlayVerts);
	}
	else if (m_isCreatingShape && m_currentShapeMode == ShapeMode::OBB)
	{
		std::vector<Vertex_PCU> overlayVerts;

		Vec2 drag = m_dragEnd - m_dragStart;
		float length = drag.GetLength();

		if (length > 2.f)
		{
			Vec2 forward = drag.GetNormalized();
			Vec2 center = (m_dragStart + m_dragEnd) * 0.5f;
			Vec2 halfDims = Vec2(length * 0.5f, 20.f);

			OBB2 drawnOBB = OBB2(center, forward, halfDims);
			AddVertsForOBB2D(overlayVerts, drawnOBB, Rgba8(255, 255, 255, 80));
		}

		g_theRenderer->DrawVertexArray(overlayVerts);
	}
	else if (m_isCreatingShape && m_currentShapeMode == ShapeMode::LINESEGMENT)
	{
		std::vector<Vertex_PCU> overlayVerts;

		AddVertsForLineSegment2D(overlayVerts, m_dragStart, m_dragEnd, 4.f, Rgba8(255, 255, 255, 80));

		g_theRenderer->DrawVertexArray(overlayVerts);
	}
	else if (m_isCreatingShape && m_currentShapeMode == ShapeMode::TRIANGLE)
	{
		std::vector<Vertex_PCU> overlayVerts;
		Vec2 baseStart = m_dragStart;
		Vec2 baseEnd = m_dragEnd;

		Vec2 baseCenter = (baseStart + baseEnd) * 0.5f;
		Vec2 dir = (baseEnd - baseStart);

		if (dir.GetLength() > 2.f)
		{
			Vec2 perp = dir.GetRotated90Degrees().GetNormalized();
			float height = dir.GetLength() * 0.5f;
			Vec2 tip = baseCenter + perp * height;

			AddVertsForTriangle2D(overlayVerts, baseStart, baseEnd, tip, Rgba8(255, 255, 255, 80));
		}

		g_theRenderer->DrawVertexArray(overlayVerts);
	}
	else if (m_isCreatingShape && m_currentShapeMode == ShapeMode::CAPSULE)
	{
		std::vector<Vertex_PCU> overlayVerts;

		AddVertsForCapsule2D(overlayVerts, m_dragStart, m_dragEnd, CAPSULE_RADIUS, Rgba8(255, 255, 255, 80));

		g_theRenderer->DrawVertexArray(overlayVerts);
	}
	else if (m_isCreatingShape && m_currentShapeMode == ShapeMode::PLANE)
	{
		std::vector<Vertex_PCU> overlayVerts;

		Vec2 dir = m_dragEnd - m_dragStart;
		if (dir.GetLength() > 2.f)
		{
			Vec2 normal = dir.GetRotated90Degrees().GetNormalized();
			float dist = DotProduct2D(normal, m_dragStart);

			Plane2 plane(normal, dist);
			AddVertsForPlane2D(overlayVerts, plane, Vec2::ZERO, 1000.f, 2.f, Rgba8(255, 255, 255, 80));

			Vec2 mid = (m_dragStart + m_dragEnd) * 0.5f;

			float arrowLength = 40.f;
			Vec2 tip = mid + normal * arrowLength;

			AddVertsForLineSegment2D(overlayVerts, mid, tip, 3.f, Rgba8(0, 255, 0, 200));

			Vec2 left = (tip - normal * 10.f) + normal.GetRotated90Degrees() * 6.f;
			Vec2 right = (tip - normal * 10.f) - normal.GetRotated90Degrees() * 6.f;

			AddVertsForLineSegment2D(overlayVerts, tip, left, 3.f, Rgba8::GREEN);
			AddVertsForLineSegment2D(overlayVerts, tip, right, 3.f, Rgba8::GREEN);
		}

		g_theRenderer->DrawVertexArray(overlayVerts);
	}
	// -----------------------------------------------------------------------------

	// Text
	// -----------------------------------------------------------------------------
	GameModeAndControlsText();
	// -----------------------------------------------------------------------------

	g_theRenderer->EndCamera(m_screenCamera);
}

void Game::GameModeAndControlsText() const
{
	std::vector<Vertex_PCU> textVerts;
	AABB2 gameSceneBounds = AABB2(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));
	m_font->AddVertsForTextInBox2D(textVerts, "Interactive SDFVisualizer", gameSceneBounds, 20.f, Rgba8::WHITE, 0.8f, Vec2(0.f, 0.965f));
	m_font->AddVertsForTextInBox2D(textVerts, Stringf("Shape: %s (Mouse Wheel to Change) | LMB Drag to Create | C to clear shapes", GetShapeModeName(m_currentShapeMode)), gameSceneBounds, 15.f, Rgba8::WHITE, 0.8f, Vec2(0.f, 0.945f));
	m_font->AddVertsForTextInBox2D(textVerts, Stringf("(Up/Down Arrows) Smooth Value: %0.2f", m_smoothK), gameSceneBounds, 15.f, Rgba8::WHITE, 0.8f, Vec2(0.f, 0.925f));
	m_font->AddVertsForTextInBox2D(textVerts, "Render Modes: (1) Solid, (2) Bands, (3) AntiAlias, (4) Glow", gameSceneBounds, 15.f, Rgba8::WHITE, 0.8f, Vec2(0.f, 0.905f));
	m_font->AddVertsForTextInBox2D(textVerts, "Blend Modes : (5) Min, (6) SMin, (7) Max", gameSceneBounds, 15.f, Rgba8::WHITE, 0.8f, Vec2(0.f, 0.885f));
	m_font->AddVertsForTextInBox2D(textVerts, Stringf("Surface: %s (%.1f) (Z)", GetSurfaceModeName(m_surfaceMode), m_surfaceParam), gameSceneBounds, 15.f, Rgba8::WHITE, 0.8f, Vec2(0.f, 0.865f));
	m_font->AddVertsForTextInBox2D(textVerts, "(M) Toggle moon visibility", gameSceneBounds, 15.f, Rgba8::WHITE, 0.8f, Vec2(0.f, 0.845f));
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED);
	g_theRenderer->BindTexture(&m_font->GetTexture());
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray(textVerts);
}

void Game::Shutdown()
{
	delete m_constantBuffer;
	m_constantBuffer = nullptr;

	delete m_globalsCB;
	m_globalsCB = nullptr;
}

void Game::KeyInputPresses(float deltaSeconds)
{
	// Clearing shapes
	if (g_theInput->WasKeyJustPressed('C'))
	{
		ClearShapes();
	}

	// Changing shapes
	int wheelDelta = g_theInput->GetMouseWheelDelta();
	if (wheelDelta != 0.f)
	{
		int currentShape = static_cast<int>(m_currentShapeMode);
		int count = static_cast<int>(ShapeMode::SHAPE_COUNT);

		if (wheelDelta > 0)
		{
			currentShape = (currentShape + 1) % count;
		}
		else
		{
			currentShape = (currentShape - 1 + count) % count;
		}

		m_currentShapeMode = static_cast<ShapeMode>(currentShape);
	}

	// Adjusting smooth min value
	if (g_theInput->IsKeyDown(KEYCODE_UPARROW))
	{
		m_smoothK += SMOOTH_SPEED * deltaSeconds;
	}
	if (g_theInput->IsKeyDown(KEYCODE_DOWNARROW))
	{
		m_smoothK -= SMOOTH_SPEED * deltaSeconds;
	}

	// Switch Render modes
	if (g_theInput->WasKeyJustPressed('1'))
	{
		m_currentRenderMode = RenderMode::SOLID;
	}
	if (g_theInput->WasKeyJustPressed('2'))
	{
		m_currentRenderMode = RenderMode::BANDS;
	}
	if (g_theInput->WasKeyJustPressed('3'))
	{
		m_currentRenderMode = RenderMode::ANTIALIASING;
	}
	if (g_theInput->WasKeyJustPressed('4'))
	{
		m_currentRenderMode = RenderMode::GLOW;
	}

	// Switch Blend modes
	if (g_theInput->WasKeyJustPressed('5'))
	{
		m_blendMode = SDFBlendMode::MIN;
	}
	if (g_theInput->WasKeyJustPressed('6'))
	{
		m_blendMode = SDFBlendMode::SMOOTHMIN;
	}
	if (g_theInput->WasKeyJustPressed('7'))
	{
		m_blendMode = SDFBlendMode::MAX;
	}

	// Switch Surface modes
	if (g_theInput->WasKeyJustPressed('Z'))
	{
		int surfaceMode = static_cast<int>(m_surfaceMode);
		surfaceMode = (surfaceMode + 1) % static_cast<int>(SDFSurfaceMode::SURFACE_COUNT);
		m_surfaceMode = static_cast<SDFSurfaceMode>(surfaceMode);
	}
	if (g_theInput->IsKeyDown(KEYCODE_LEFTARROW))
	{
		m_surfaceParam -= 30.f * deltaSeconds;
	}
	if (g_theInput->IsKeyDown(KEYCODE_RIGHTARROW))
	{
		m_surfaceParam += 30.f * deltaSeconds;
	}
	m_surfaceParam = GetClamped(m_surfaceParam, 0.f, 200.f);

	// Moon visibility toggle
	if (g_theInput->WasKeyJustPressed('M'))
	{
		m_showMoon = !m_showMoon;
	}
}

char const* Game::GetShapeModeName(ShapeMode mode) const
{
	switch (mode)
	{
		case ShapeMode::AABB:        return "AABB";
		case ShapeMode::CIRCLE:      return "Circle";
		case ShapeMode::OBB:         return "OBB";
		case ShapeMode::LINESEGMENT: return "Line Segment";
		case ShapeMode::TRIANGLE:    return "Triangle";
		case ShapeMode::CAPSULE:     return "Capsule";
		case ShapeMode::PLANE:       return "Plane";
		default:                     return "Unknown";
	}
}

const char* Game::GetSurfaceModeName(SDFSurfaceMode mode) const
{
	switch (mode)
	{
		case SDFSurfaceMode::HARD_EDGE: return "Hard";
		case SDFSurfaceMode::ROUNDED:   return "Rounded";
		case SDFSurfaceMode::ONION:     return "Onion";
		default: return "Unknown";
	}
}

void Game::ClearShapes()
{
	m_boxes.clear();
	m_circles.clear();
	m_obbs.clear();
	m_lineSegments.clear();
	m_triangles.clear();
	m_capsules.clear();
	m_planes.clear();
}

void Game::AdjustForPauseAndTimeDistortion(float deltaSeconds) {

	UNUSED(deltaSeconds);

	//if (g_theInput->IsKeyDown('T'))
	//{
	//	m_gameClock.SetTimeScale(0.1);
	//}

	if (g_theInput->WasKeyJustPressed('P'))
	{
		m_gameClock.TogglePause();
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
	{
		g_theEventSystem->FireEvent("Quit");
	}
}

void Game::UpdateCameras()
{
	m_screenCamera.SetOrthoView(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));
}

void Game::SetSDFShapesConstants() const
{
	SDFShapes sdfConstants = {};

	// DISCS
	int circleCount = GetMin(static_cast<int>(m_circles.size()), MAX_SDF_CIRCLES);
	sdfConstants.numCircles.x = static_cast<float>(circleCount);
	for (int circleIndex = 0; circleIndex < circleCount; ++circleIndex)
	{
		SDFCircle const& circle = m_circles[circleIndex];
		sdfConstants.circleData[circleIndex] = Vec4(circle.m_center.x, circle.m_center.y, circle.m_radius, 0.f);
	}

	// AABBs
	int count = static_cast<int>(m_boxes.size());
	count = GetMin(count, MAX_SDF_BOXES);
	sdfConstants.numBoxes.x = static_cast<float>(count);
	for (int boxIndex = 0; boxIndex < count; ++boxIndex)
	{
		Vec2 center = m_boxes[boxIndex].m_center;
		Vec2 halfDims = m_boxes[boxIndex].m_halfDims;

		sdfConstants.boxData[boxIndex] = Vec4(center.x, center.y, halfDims.x, halfDims.y);
	}

	// OBBs
	int obbCount = GetMin(static_cast<int>(m_obbs.size()), MAX_SDF_BOXES);
	sdfConstants.numOBBs.x = static_cast<float>(obbCount);
	for (int obbIndex = 0; obbIndex < obbCount; ++obbIndex)
	{
		SDFOrientedBox const& obb = m_obbs[obbIndex];

		float cos = cosf(obb.m_rotationRadians);
		float sine = sinf(obb.m_rotationRadians);
		Vec2 right = Vec2(cos, sine);
		Vec2 up = Vec2(-sine, cos);

		sdfConstants.obbData[obbIndex * 2 + 0] = Vec4(obb.m_center.x, obb.m_center.y, obb.m_halfDims.x, obb.m_halfDims.y);
		sdfConstants.obbData[obbIndex * 2 + 1] = Vec4(right.x, right.y, up.x, up.y);
	}

	// LINE SEGMENTS
	int segmentCount = GetMin(static_cast<int>(m_lineSegments.size()), MAX_SDF_SEGMENTS);
	sdfConstants.numLineSegments.x = static_cast<float>(segmentCount);
	for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
	{
		SDFLineSegment const& lineSegment = m_lineSegments[segmentIndex];

		Vec2 start = lineSegment.m_start;
		Vec2 end = lineSegment.m_end;

		sdfConstants.lineSegmentData[segmentIndex] = Vec4(start.x, start.y, end.x, end.y);
	}

	// TRIANGLES
	int triCount = GetMin(static_cast<int>(m_triangles.size()), MAX_SDF_TRIANGLES);
	sdfConstants.numTriangles.x = static_cast<float>(triCount);

	for (int triIndex = 0; triIndex < triCount; ++triIndex)
	{
		SDFTriangle const& tri = m_triangles[triIndex];

		sdfConstants.triangleData[triIndex * 2 + 0] = Vec4(tri.m_ccw0.x, tri.m_ccw0.y, tri.m_ccw1.x, tri.m_ccw1.y);
		sdfConstants.triangleData[triIndex * 2 + 1] = Vec4(tri.m_ccw2.x, tri.m_ccw2.y, 0.f, 0.f);
	}

	// CAPSULES
	int capsuleCount = GetMin(static_cast<int>(m_capsules.size()), MAX_SDF_CAPSULES);
	sdfConstants.numCapsules.x = static_cast<float>(capsuleCount);

	for (int capsuleIndex = 0; capsuleIndex < capsuleCount; ++capsuleIndex)
	{
		SDFCapsule const& capsule = m_capsules[capsuleIndex];

		sdfConstants.capsuleData[capsuleIndex * 2 + 0] = Vec4(capsule.m_start.x, capsule.m_start.y, capsule.m_end.x, capsule.m_end.y);
		sdfConstants.capsuleData[capsuleIndex * 2 + 1] = Vec4(capsule.m_radius, 0.f, 0.f, 0.f);
	}

	// PLANES
	int planeCount = GetMin(static_cast<int>(m_planes.size()), MAX_SDF_PLANES);
	sdfConstants.numPlanes.x = static_cast<float>(planeCount);

	for (int planeIndex = 0; planeIndex < planeCount; ++planeIndex)
	{
		SDFPlane const& plane = m_planes[planeIndex];
		sdfConstants.planeData[planeIndex] = Vec4(plane.m_planeNormal.x, plane.m_planeNormal.y, plane.m_distanceFromOrigin, 0.f);
	}

	// RENDER FILTER
	sdfConstants.renderParams = Vec4(m_smoothK, static_cast<float>(m_currentRenderMode), static_cast<float>(m_blendMode), ANTI_ALIAS_THRESHOLD);

	// SURFACE FILTER
	sdfConstants.surfaceParams = Vec4(static_cast<float>(m_surfaceMode), m_surfaceParam, 0.f, 0.f);

	g_theRenderer->CopyCPUToGPU((void*)&sdfConstants, sizeof(sdfConstants), m_constantBuffer);
	g_theRenderer->BindConstantBuffer(4, m_constantBuffer);
}

void Game::SetGlobalsConstants() const
{
	SDFGlobals globals;

	globals.moonParams = Vec4(MOON_R1, MOON_R2, m_moonTime, m_showMoon ? 1.f : 0.f);

	g_theRenderer->CopyCPUToGPU(&globals, sizeof(globals), m_globalsCB);
	g_theRenderer->BindConstantBuffer(5, m_globalsCB);
}

void Game::UpdateMouseDrag()
{
	Vec2 mouseClient = g_theInput->GetCursorClientPosition();
	Vec2 mouseWorld = m_screenCamera.GetClientToWorld(mouseClient, g_theWindow->GetClientDimensions());

	if (g_theInput->WasKeyJustPressed(KEYCODE_LEFT_MOUSE))
	{
		m_isCreatingShape = true;
		m_dragStart = mouseWorld;
		m_dragEnd = mouseWorld;
	}

	if (g_theInput->IsKeyDown(KEYCODE_LEFT_MOUSE) && m_isCreatingShape)
	{
		m_dragEnd = mouseWorld;
	}

	if (g_theInput->WasKeyJustReleased(KEYCODE_LEFT_MOUSE))
	{
		m_isCreatingShape = false;

		if (m_currentShapeMode == ShapeMode::AABB)
		{
			CreateBoxFromDrag();
		}
		else if (m_currentShapeMode == ShapeMode::CIRCLE)
		{
			CreateCircleFromDrag();
		}
		else if (m_currentShapeMode == ShapeMode::OBB)
		{
			CreateOBBFromDrag();
		}
		else if (m_currentShapeMode == ShapeMode::LINESEGMENT)
		{
			CreateLineSegmentFromDrag();
		}
		else if (m_currentShapeMode == ShapeMode::TRIANGLE)
		{
			CreateTriangleFromDrag();
		}
		else if (m_currentShapeMode == ShapeMode::CAPSULE)
		{
			CreateCapsuleFromDrag();
		}
		else if (m_currentShapeMode == ShapeMode::PLANE)
		{
			CreatePlaneFromDrag();
		}
	}
}

void Game::CreateBoxFromDrag()
{
	Vec2 center = (m_dragStart + m_dragEnd) * 0.5f;
	Vec2 halfDims = Abs(m_dragEnd - m_dragStart) * 0.5f;

	if (halfDims.x < 2.f || halfDims.y < 2.f)
	{
		return;
	}

	m_boxes.push_back({ center, halfDims });
}

void Game::CreateCircleFromDrag()
{
	Vec2 center = m_dragStart;
	float radius = (m_dragEnd - m_dragStart).GetLength();

	if (radius < 2.f)
	{
		return;
	}

	m_circles.push_back({ center, radius });
}

void Game::CreateOBBFromDrag()
{
	Vec2 drag = m_dragEnd - m_dragStart;
	float length = drag.GetLength();

	if (length < 2.f)
	{
		return;
	}

	Vec2 forward = drag.GetNormalized();
	float rotation = atan2f(forward.y, forward.x);

	Vec2 center = (m_dragStart + m_dragEnd) * 0.5f;
	Vec2 halfDims = Vec2(length * 0.5f, 20.f);

	m_obbs.push_back({ center, halfDims, rotation });
}

void Game::CreateLineSegmentFromDrag()
{
	Vec2 segmentStart = m_dragStart;
	Vec2 segmentEnd = m_dragEnd;

	m_lineSegments.push_back({ segmentStart, segmentEnd });
}

void Game::CreateTriangleFromDrag()
{
	Vec2 baseStart = m_dragStart;
	Vec2 baseEnd = m_dragEnd;

	Vec2 baseCenter = (baseStart + baseEnd) * 0.5f;
	Vec2 dir = (baseEnd - baseStart);

	if (dir.GetLength() < 2.f)
	{
		return;
	}

	Vec2 perp = dir.GetRotated90Degrees().GetNormalized();
	float height = dir.GetLength() * 0.5f;
	Vec2 tip = baseCenter + perp * height;

	SDFTriangle tri;
	tri.m_ccw0 = baseStart;
	tri.m_ccw1 = baseEnd;
	tri.m_ccw2 = tip;

	m_triangles.push_back(tri);
}

void Game::CreateCapsuleFromDrag()
{
	Vec2 segmentStart = m_dragStart;
	Vec2 segmentEnd = m_dragEnd;

	float length = (segmentEnd - segmentStart).GetLength();
	if (length < 2.f)
	{
		return;
	}

	m_capsules.push_back({ segmentStart, segmentEnd, CAPSULE_RADIUS });
}

void Game::CreatePlaneFromDrag()
{
	Vec2 planeStart = m_dragStart;
	Vec2 planeEnd = m_dragEnd;

	Vec2 planeDirection = planeEnd - planeStart;
	float planeLength = planeDirection.GetLength();

	if (planeLength < 2.f)
	{
		return;
	}

	Vec2 planeNormal = planeDirection.GetRotated90Degrees().GetNormalized();
	float distance = DotProduct2D(planeNormal, planeStart);

	m_planes.push_back({ planeNormal, distance });
}
