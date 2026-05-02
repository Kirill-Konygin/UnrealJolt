#include "JoltLayerTable.h"
#include "JoltSettings.h"

FJoltLayerTable FJoltLayerTable::BuildFromSettings(const UJoltSettings& Settings)
{
	FJoltLayerTable Table;

	// Build broadphase layer name list + name -> index map.
	Table.BroadphaseLayerNames.Reserve(Settings.BroadphaseLayers.Num());
	Table.BroadphaseLayerNamesAnsi.Reserve(Settings.BroadphaseLayers.Num());
	for (const FJoltBroadphaseLayer& bp : Settings.BroadphaseLayers)
	{
		const int32 idx = Table.BroadphaseLayerNames.Add(bp.Name);
		Table.NameToBroadphaseLayer.Add(bp.Name, idx);

		// ANSI cache for the Jolt profiler hook (returns const char*). Stored null-terminated.
		const FString	  nameStr = bp.Name.ToString();
		const auto		  ansi = StringCast<ANSICHAR>(*nameStr);
		TArray<ANSICHAR>& dst = Table.BroadphaseLayerNamesAnsi.AddDefaulted_GetRef();
		dst.Append(ansi.Get(), ansi.Length());
		dst.Add('\0');
	}

	// Build object layer name list + name -> index map.
	Table.ObjectLayerNames.Reserve(Settings.ObjectLayers.Num());
	for (const FJoltObjectLayer& ol : Settings.ObjectLayers)
	{
		const int32 idx = Table.ObjectLayerNames.Add(ol.Name);
		Table.NameToObjectLayer.Add(ol.Name, idx);
	}

	const int32 nObj = Table.ObjectLayerNames.Num();
	const int32 nBp = Table.BroadphaseLayerNames.Num();

	// Fallback broadphase index if an object layer references a broadphase that doesn't exist;
	// keeps the table self-consistent even when the user has half-edited the settings.
	const uint8 fallbackBroadphase = 0;

	Table.ObjectToBroadphase.SetNumZeroed(nObj);
	for (int32 i = 0; i < nObj; ++i)
	{
		const FJoltObjectLayer& ol = Settings.ObjectLayers[i];
		const int32* bpIdx = Table.NameToBroadphaseLayer.Find(ol.BroadphaseLayer);
		Table.ObjectToBroadphase[i] = bpIdx != nullptr ? static_cast<uint8>(*bpIdx) : fallbackBroadphase;
	}

	// Build symmetric object-vs-object collision mask. PostEditChangeProperty normally keeps this
	// symmetric, but OR both directions here so a half-edited DefaultJolt.ini still produces a
	// valid table.
	Table.ObjectCollisionMask.SetNumZeroed(nObj * nObj);
	for (int32 i = 0; i < nObj; ++i)
	{
		const FJoltObjectLayer& ol = Settings.ObjectLayers[i];
		for (const FName& otherName : ol.CollidesWith)
		{
			const int32* j = Table.NameToObjectLayer.Find(otherName);
			if (j == nullptr)
			{
				continue;
			}
			Table.ObjectCollisionMask[i * nObj + *j] = 1;
			Table.ObjectCollisionMask[*j * nObj + i] = 1;
		}
	}

	// Object-vs-broadphase mask: an object layer collides with a broadphase layer if it collides
	// with any object layer mapped into that broadphase.
	Table.ObjectVsBroadphaseMask.SetNumZeroed(nObj * nBp);
	for (int32 i = 0; i < nObj; ++i)
	{
		for (int32 j = 0; j < nObj; ++j)
		{
			if (Table.ObjectCollisionMask[i * nObj + j] == 0)
			{
				continue;
			}
			const uint8 bp = Table.ObjectToBroadphase[j];
			if (bp < nBp)
			{
				Table.ObjectVsBroadphaseMask[i * nBp + bp] = 1;
			}
		}
	}

	return Table;
}
