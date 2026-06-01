#pragma once
#include "Game/GameCommon.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Vertex_PCU.h"
#include "Engine/Math/IntVec4.hpp"
#include <string>
// -----------------------------------------------------------------------------
class Shader;
class ConstantBuffer;
class BitmapFont;
// -----------------------------------------------------------------------------
constexpr int   MAX_SDF_BOXES = 32;
constexpr int   MAX_SDF_CIRCLES = 32;
constexpr int   MAX_SDF_SEGMENTS = 32;
constexpr int   MAX_SDF_TRIANGLES = 32;
constexpr int   MAX_SDF_CAPSULES = 32;
constexpr int   MAX_SDF_PLANES = 32;
constexpr float SMOOTH_SPEED = 60.f;
constexpr float ANTI_ALIAS_THRESHOLD = 15.f;
constexpr float CAPSULE_RADIUS = 30.f;
constexpr float MOON_R1 = 120.f;
constexpr float MOON_R2 = 100.f;
// -----------------------------------------------------------------------------
enum class ShapeMode
{
	AABB,
	CIRCLE,
	OBB,
	LINESEGMENT,
	TRIANGLE,
	CAPSULE,
	PLANE,
	SHAPE_COUNT
};
// -----------------------------------------------------------------------------
enum class RenderMode
{
	SOLID,
	BANDS,
	ANTIALIASING,
	GLOW
};
// -----------------------------------------------------------------------------
enum class SDFBlendMode
{
	MIN,
	SMOOTHMIN,
	MAX
};
// -----------------------------------------------------------------------------
enum class SDFSurfaceMode
{
	HARD_EDGE,
	ROUNDED,
	ONION,
	SURFACE_COUNT
};
// -----------------------------------------------------------------------------
struct SDFShapes
{
	Vec4 numCircles;
	Vec4 circleData[MAX_SDF_CIRCLES];

	Vec4 numBoxes;
	Vec4 boxData[MAX_SDF_BOXES];

	Vec4 numOBBs;
	Vec4 obbData[MAX_SDF_BOXES * 2];

	Vec4 numLineSegments;
	Vec4 lineSegmentData[MAX_SDF_SEGMENTS];

	Vec4 numTriangles;
	Vec4 triangleData[MAX_SDF_TRIANGLES * 2];

	Vec4 numCapsules;
	Vec4 capsuleData[MAX_SDF_CAPSULES * 2];

	Vec4 numPlanes;
	Vec4 planeData[MAX_SDF_PLANES];

	Vec4 renderParams;
	Vec4 surfaceParams;
};
// -----------------------------------------------------------------------------
struct SDFGlobals
{
	Vec4 moonParams; 
};
// -----------------------------------------------------------------------------
struct SDFBox
{
	Vec2 m_center;
	Vec2 m_halfDims;
};
// -----------------------------------------------------------------------------
struct SDFOrientedBox 
{
	Vec2 m_center;
	Vec2 m_halfDims;
	float m_rotationRadians = 0.f;
};
// -----------------------------------------------------------------------------
struct SDFLineSegment
{
	Vec2 m_start;
	Vec2 m_end;
};
// -----------------------------------------------------------------------------
struct SDFCircle
{
	Vec2 m_center;
	float m_radius = 0.f;
};
// -----------------------------------------------------------------------------
struct SDFTriangle
{
	Vec2 m_ccw0;
	Vec2 m_ccw1;
	Vec2 m_ccw2;
};
// -----------------------------------------------------------------------------
struct SDFCapsule
{
	Vec2 m_start;
	Vec2 m_end;
	float m_radius = 0.f;
};
// -----------------------------------------------------------------------------
struct SDFPlane
{
	Vec2 m_planeNormal;
	float m_distanceFromOrigin = 0.f;
};
// -----------------------------------------------------------------------------
class Game
{
public:
	App* m_app;
	Game(App* owner);
	~Game();
	void StartUp();

	void Update();
	void UpdateCameras();

	void UpdateMouseDrag();
	void CreateBoxFromDrag();
	void CreateCircleFromDrag();
	void CreateOBBFromDrag();
	void CreateLineSegmentFromDrag();
	void CreateTriangleFromDrag();
	void CreateCapsuleFromDrag();
	void CreatePlaneFromDrag();

	void SetSDFShapesConstants() const;
	void SetGlobalsConstants() const;
	void Render() const;
	void GameModeAndControlsText() const;

	void Shutdown();

	char const* GetShapeModeName(ShapeMode mode) const;
	const char* GetSurfaceModeName(SDFSurfaceMode mode) const;
	void ClearShapes();

	void KeyInputPresses(float deltaSeconds);
	void AdjustForPauseAndTimeDistortion(float deltaSeconds);

private:
	Camera		m_screenCamera;
	Clock		m_gameClock;
	BitmapFont* m_font = nullptr;

	ConstantBuffer* m_constantBuffer = nullptr;
	ConstantBuffer* m_globalsCB = nullptr;
	Shader* m_sdfShader = nullptr;

	ShapeMode m_currentShapeMode = ShapeMode::AABB;
	RenderMode m_currentRenderMode = RenderMode::SOLID;
	SDFBlendMode m_blendMode = SDFBlendMode::MIN;
	std::vector<SDFBox> m_boxes;
	std::vector<SDFCircle> m_circles;
	std::vector<SDFOrientedBox> m_obbs;
	std::vector<SDFLineSegment> m_lineSegments;
	std::vector<SDFTriangle> m_triangles;
	std::vector<SDFCapsule> m_capsules;
	std::vector<SDFPlane> m_planes;
	float m_smoothK = 25.f;
	SDFSurfaceMode m_surfaceMode = SDFSurfaceMode::HARD_EDGE;
	float m_surfaceParam = 10.f;

	bool m_showMoon = true;
	float m_moonTime = 0.f;

	bool m_isCreatingShape = false;
	Vec2 m_dragStart;
	Vec2 m_dragEnd;
};