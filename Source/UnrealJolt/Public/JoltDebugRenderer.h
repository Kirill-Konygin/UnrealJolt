#pragma once

#include "Components/TextRenderComponent.h"
#include "UnrealJolt/Helpers.h"
#include "UnrealJolt/JoltMain.h"
#include "JoltSettings.h"
#include "DrawDebugHelpers.h"

#include <Jolt/Physics/Body/BodyFilter.h>

class UEJoltBodyDrawFilter : public JPH::BodyDrawFilter
{
public:
	bool bDrawStatic = true;
	bool bDrawDynamic = true;
	bool bDrawKinematic = true;
	bool bDrawHeightField = true;

	virtual bool ShouldDraw(const JPH::Body& inBody) const override
	{
		// HeightField bodies are always Static, so check shape type first
		// to give heightfields an independent toggle
		if (inBody.GetShape()->GetType() == JPH::EShapeType::HeightField)
		{
			return bDrawHeightField;
		}

		switch (inBody.GetMotionType())
		{
		case JPH::EMotionType::Static:
			return bDrawStatic;
		case JPH::EMotionType::Dynamic:
			return bDrawDynamic;
		case JPH::EMotionType::Kinematic:
			return bDrawKinematic;
		default:
			return true;
		}
	}
};

class UEJoltDebugRenderer final : public JPH::DebugRendererSimple
{
private:
	UWorld* World;
	bool bDrawPersistent = false;

	UEJoltBodyDrawFilter DrawFilter;

	bool bStaticBodiesDrawn = false;
	bool bWasEnabled = false;
	bool bPrevDrawStatic = true;
	bool bPrevDrawHeightField = true;

public:
	UEJoltDebugRenderer(UWorld* world)
	{
		World = world;
	}

	void OnDebugRenderDisabled()
	{
		if (bWasEnabled)
		{
			FlushPersistentDebugLines(World);
			bStaticBodiesDrawn = false;
			bWasEnabled = false;
		}
	}

	void DrawBodiesFiltered(JPH::PhysicsSystem* PhysicsSystem, const JPH::BodyManager::DrawSettings& Settings, const UJoltSettings* JoltSettings)
	{
		bWasEnabled = true;

		// Detect if static/heightfield sub-toggles changed
		if (JoltSettings->bDebugDrawStaticBodies != bPrevDrawStatic ||
			JoltSettings->bDebugDrawHeightFields != bPrevDrawHeightField)
		{
			FlushPersistentDebugLines(World);
			bStaticBodiesDrawn = false;
			bPrevDrawStatic = JoltSettings->bDebugDrawStaticBodies;
			bPrevDrawHeightField = JoltSettings->bDebugDrawHeightFields;
		}

		bool bWantStatics = JoltSettings->bDebugDrawStaticBodies || JoltSettings->bDebugDrawHeightFields;

		// Pass 1: Draw static/heightfield bodies with persistent lines (only once)
		if (bWantStatics && !bStaticBodiesDrawn)
		{
			DrawFilter.bDrawStatic = JoltSettings->bDebugDrawStaticBodies;
			DrawFilter.bDrawDynamic = false;
			DrawFilter.bDrawKinematic = false;
			DrawFilter.bDrawHeightField = JoltSettings->bDebugDrawHeightFields;

			bDrawPersistent = true;
			PhysicsSystem->DrawBodies(Settings, this, &DrawFilter);
			bDrawPersistent = false;

			bStaticBodiesDrawn = true;
		}

		// If statics are no longer wanted but were drawn, flush them
		if (!bWantStatics && bStaticBodiesDrawn)
		{
			FlushPersistentDebugLines(World);
			bStaticBodiesDrawn = false;
		}

		// Pass 2: Draw dynamic/kinematic bodies every frame
		if (JoltSettings->bDebugDrawDynamicBodies || JoltSettings->bDebugDrawKinematicBodies)
		{
			DrawFilter.bDrawStatic = false;
			DrawFilter.bDrawDynamic = JoltSettings->bDebugDrawDynamicBodies;
			DrawFilter.bDrawKinematic = JoltSettings->bDebugDrawKinematicBodies;
			DrawFilter.bDrawHeightField = false;

			PhysicsSystem->DrawBodies(Settings, this, &DrawFilter);
		}
	}

	virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override
	{
		if (!World)
		{
			UE_LOG(JoltSubSystemLogs, Warning, TEXT("JoltPhysicsSubSystem::DebugRenderer World is null."));
			return;
		}

		DrawDebugLine(World,
			JoltHelpers::ToUEPos(inFrom),
			JoltHelpers::ToUEPos(inTo),
			JoltHelpers::ToUEColor(inColor),
			bDrawPersistent,
			bDrawPersistent ? 0.0f : -1.0f);
	}

	virtual void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) override
	{
		if (!World)
		{
			UE_LOG(JoltSubSystemLogs, Warning, TEXT("JoltPhysicsSubSystem::DebugRenderer World is null."));
			return;
		}
		FVector V1 = JoltHelpers::ToUEPos(inV1);
		FVector V2 = JoltHelpers::ToUEPos(inV2);
		FVector V3 = JoltHelpers::ToUEPos(inV3);

		FColor Color = JoltHelpers::ToUEColor(inColor);
		float LifeTime = bDrawPersistent ? 0.0f : -1.0f;

		DrawDebugLine(World, V1, V2, Color, bDrawPersistent, LifeTime, 0, 1);
		DrawDebugLine(World, V2, V3, Color, bDrawPersistent, LifeTime, 0, 1);
		DrawDebugLine(World, V3, V1, Color, bDrawPersistent, LifeTime, 0, 1);
	}

	virtual void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor, float inHeight) override
	{
		if (!World)
		{
			UE_LOG(JoltSubSystemLogs, Warning, TEXT("JoltPhysicsSubSystem::DebugRenderer World is null."));
			return;
		}

		FVector Position = JoltHelpers::ToUEPos(inPosition);
		FColor	Color = JoltHelpers::ToUEColor(inColor);

		FString TextString(inString.data());

		UTextRenderComponent* TextRenderComponent = NewObject<UTextRenderComponent>(World);
		if (!TextRenderComponent)
		{
			UE_LOG(JoltSubSystemLogs, Warning, TEXT("Failed to create UTextRenderComponent."));
			return;
		}

		TextRenderComponent->SetText(FText::FromString(TextString));
		TextRenderComponent->SetTextRenderColor(Color);
		TextRenderComponent->SetWorldLocation(Position);
		TextRenderComponent->SetWorldScale3D(FVector(inHeight / 100.0f));

		TextRenderComponent->RegisterComponentWithWorld(World);
	}
};
