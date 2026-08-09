#include "OctoOdyssey/OctoTuning.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"

const TCHAR* const OctoTuning::IniSection = TEXT("OctoTuning");

namespace
{
    FOctoTuningParam FloatParam(
        const TCHAR* Name, const TCHAR* Category, float FOctoTuning::* Member,
        float Min, float Max, float Step, bool bRequiresRestart = false)
    {
        FOctoTuningParam P;
        P.Name             = Name;
        P.Category         = Category;
        P.Kind             = EOctoParamKind::Float;
        P.FloatMember      = Member;
        P.Min              = Min;
        P.Max              = Max;
        P.Step             = Step;
        P.bRequiresRestart = bRequiresRestart;
        return P;
    }

    FOctoTuningParam BoolParam(
        const TCHAR* Name, const TCHAR* Category, bool FOctoTuning::* Member,
        bool bRequiresRestart = false)
    {
        FOctoTuningParam P;
        P.Name             = Name;
        P.Category         = Category;
        P.Kind             = EOctoParamKind::Bool;
        P.BoolMember       = Member;
        P.Min              = 0.f;
        P.Max              = 1.f;
        P.Step             = 1.f;
        P.bRequiresRestart = bRequiresRestart;
        return P;
    }
}

TArrayView<const FOctoTuningParam> OctoTuning::GetParams()
{
    // Display order == table order, and the menu inserts a heading whenever the
    // Category string changes, so keep each category's rows contiguous.
    static const TArray<FOctoTuningParam> Params =
    {
        // ---- Geometry: baked at spawn, restart-only ------------------------
        FloatParam(TEXT("SphereRadius"),           TEXT("Geometry"), &FOctoTuning::SphereRadius,           10.f,  150.f,  5.f,   true),
        FloatParam(TEXT("ArmRadius"),              TEXT("Geometry"), &FOctoTuning::ArmRadius,               2.f,   40.f,  1.f,   true),
        FloatParam(TEXT("ArmMeshRadiusScale"),     TEXT("Geometry"), &FOctoTuning::ArmMeshRadiusScale,      0.1f,   1.5f, 0.05f, true),
        FloatParam(TEXT("ArmHalfHeight"),          TEXT("Geometry"), &FOctoTuning::ArmHalfHeight,          10.f,  150.f,  5.f,   true),
        FloatParam(TEXT("ArmMaxExtension"),        TEXT("Geometry"), &FOctoTuning::ArmMaxExtension,         5.f,  200.f,  5.f,   true),
        FloatParam(TEXT("ArmAngleOffsetDegrees"),  TEXT("Geometry"), &FOctoTuning::ArmAngleOffsetDegrees,   0.f,   45.f,  2.5f,  true),
        FloatParam(TEXT("ArmSweepSkin"),           TEXT("Geometry"), &FOctoTuning::ArmSweepSkin,            0.f,   10.f,  0.5f,  true),

        // ---- Arms ----------------------------------------------------------
        FloatParam(TEXT("ExtendSpeed"),            TEXT("Arms"),     &FOctoTuning::ExtendSpeed,           100.f, 3000.f, 25.f),
        FloatParam(TEXT("RetractSpeed"),           TEXT("Arms"),     &FOctoTuning::RetractSpeed,          100.f, 3000.f, 25.f),
        FloatParam(TEXT("ArmGripFraction"),        TEXT("Arms"),     &FOctoTuning::ArmGripFraction,         0.f,    1.f,  0.05f),
        BoolParam (TEXT("bPushAtImpactPoint"),     TEXT("Arms"),     &FOctoTuning::bPushAtImpactPoint),
        BoolParam (TEXT("bSweepPhysicsBodies"),    TEXT("Arms"),     &FOctoTuning::bSweepPhysicsBodies),

        // ---- Body ----------------------------------------------------------
        BoolParam (TEXT("bBodySpring"),            TEXT("Body"),     &FOctoTuning::bBodySpring),
        FloatParam(TEXT("BodySpringFrequencyHz"),  TEXT("Body"),     &FOctoTuning::BodySpringFrequencyHz,   0.5f,   8.f,  0.25f),
        FloatParam(TEXT("BodySpringDamping"),      TEXT("Body"),     &FOctoTuning::BodySpringDamping,       0.05f,  2.f,  0.05f),
        FloatParam(TEXT("BodyLagScale"),           TEXT("Body"),     &FOctoTuning::BodyLagScale,            0.f,    2.f,  0.05f),
        FloatParam(TEXT("BodyMaxDeflection"),      TEXT("Body"),     &FOctoTuning::BodyMaxDeflection,       0.f,  120.f,  5.f),
        FloatParam(TEXT("BodyGravityScale"),       TEXT("Body"),     &FOctoTuning::BodyGravityScale,        0.f,    2.f,  0.05f),
        FloatParam(TEXT("BodyBendTaper"),          TEXT("Body"),     &FOctoTuning::BodyBendTaper,           0.f,    1.f,  0.05f),
        FloatParam(TEXT("BodyMaxBendDegrees"),     TEXT("Body"),     &FOctoTuning::BodyMaxBendDegrees,      0.f,   90.f,  5.f),
        BoolParam (TEXT("bHeadCollision"),         TEXT("Body"),     &FOctoTuning::bHeadCollision),
        FloatParam(TEXT("HeadCollisionOffsetX"),   TEXT("Body"),     &FOctoTuning::HeadCollisionOffsetX,  -200.f,  200.f, 5.f),
        FloatParam(TEXT("HeadCollisionRestitution"), TEXT("Body"),   &FOctoTuning::HeadCollisionRestitution, 0.f,   1.f,  0.05f),
        FloatParam(TEXT("HeadCollisionFriction"),  TEXT("Body"),     &FOctoTuning::HeadCollisionFriction,   0.f,    1.f,  0.05f),
        BoolParam (TEXT("bHeadSquash"),            TEXT("Body"),     &FOctoTuning::bHeadSquash),
        FloatParam(TEXT("HeadSquashMax"),          TEXT("Body"),     &FOctoTuning::HeadSquashMax,           0.f,    0.8f, 0.05f),
        FloatParam(TEXT("HeadSquashFullSpeed"),    TEXT("Body"),     &FOctoTuning::HeadSquashFullSpeed,   200.f, 3000.f, 50.f),
        FloatParam(TEXT("HeadSquashFrequencyHz"),  TEXT("Body"),     &FOctoTuning::HeadSquashFrequencyHz,   1.f,   15.f,  0.5f),
        FloatParam(TEXT("HeadSquashDamping"),      TEXT("Body"),     &FOctoTuning::HeadSquashDamping,       0.05f,  2.f,  0.05f),

        // ---- Physics -------------------------------------------------------
        FloatParam(TEXT("BodyMassKg"),             TEXT("Physics"),  &FOctoTuning::BodyMassKg,              1.f,  200.f,  1.f),
        FloatParam(TEXT("LinearDamping"),          TEXT("Physics"),  &FOctoTuning::LinearDamping,           0.f,    5.f,  0.05f),
        FloatParam(TEXT("AngularDamping"),         TEXT("Physics"),  &FOctoTuning::AngularDamping,          0.f,   10.f,  0.1f),
        FloatParam(TEXT("MaxLaunchSpeed"),         TEXT("Physics"),  &FOctoTuning::MaxLaunchSpeed,        200.f, 5000.f, 50.f),
        FloatParam(TEXT("MaxAngularSpeedDeg"),     TEXT("Physics"),  &FOctoTuning::MaxAngularSpeedDeg,      0.f, 3000.f, 30.f),
        FloatParam(TEXT("MaxDepenetrationVelocity"), TEXT("Physics"), &FOctoTuning::MaxDepenetrationVelocity, 10.f, 1000.f, 10.f),
        BoolParam (TEXT("bUseCCD"),                TEXT("Physics"),  &FOctoTuning::bUseCCD),
        FloatParam(TEXT("WorldGravityZ"),          TEXT("Physics"), &FOctoTuning::WorldGravityZ,       -3000.f,    0.f, 20.f),

        // ---- Surface -------------------------------------------------------
        BoolParam (TEXT("bOverridePhysicalMaterial"), TEXT("Surface"), &FOctoTuning::bOverridePhysicalMaterial),
        FloatParam(TEXT("SurfaceFriction"),        TEXT("Surface"),  &FOctoTuning::SurfaceFriction,         0.f,    2.f,  0.05f),
        FloatParam(TEXT("SurfaceStaticFriction"),  TEXT("Surface"),  &FOctoTuning::SurfaceStaticFriction,   0.f,    2.f,  0.05f),
        FloatParam(TEXT("SurfaceRestitution"),     TEXT("Surface"),  &FOctoTuning::SurfaceRestitution,      0.f,    1.f,  0.05f),

        // ---- Visual --------------------------------------------------------
        BoolParam (TEXT("bShowPrototypeMeshes"),   TEXT("Visual"),   &FOctoTuning::bShowPrototypeMeshes),

        // ---- Camera --------------------------------------------------------
        FloatParam(TEXT("CameraDistanceX"),        TEXT("Camera"),   &FOctoTuning::CameraDistanceX,       500.f, 5000.f, 50.f),
        FloatParam(TEXT("CameraHeightOffset"),     TEXT("Camera"),   &FOctoTuning::CameraHeightOffset,   -500.f, 1000.f, 25.f),
        FloatParam(TEXT("LeadY"),                  TEXT("Camera"),   &FOctoTuning::LeadY,                   0.f, 1000.f, 25.f),
        FloatParam(TEXT("MinCameraZ"),             TEXT("Camera"),   &FOctoTuning::MinCameraZ,          -1000.f, 2000.f, 25.f),
        FloatParam(TEXT("FollowInterpSpeed"),      TEXT("Camera"),   &FOctoTuning::FollowInterpSpeed,       0.5f,  20.f,  0.5f),
        FloatParam(TEXT("CameraFieldOfView"),      TEXT("Camera"),   &FOctoTuning::CameraFieldOfView,      30.f,  120.f,  2.5f),

        // ---- Course --------------------------------------------------------
        FloatParam(TEXT("PlayPlaneX"),             TEXT("Course"),   &FOctoTuning::PlayPlaneX,          -2000.f, 2000.f, 50.f, true),
        FloatParam(TEXT("TutorialMaxSeconds"),     TEXT("Course"),   &FOctoTuning::TutorialMaxSeconds,      0.f,   15.f,  0.5f, true),
    };

    return Params;
}

FString OctoTuning::ToJsonString(const FOctoTuning& Tuning)
{
    TArrayView<const FOctoTuningParam> Params = GetParams();

    FString Json = TEXT("{");
    for (int32 i = 0; i < Params.Num(); i++)
    {
        const FOctoTuningParam& P = Params[i];

        FString Value;
        if (P.Kind == EOctoParamKind::Bool)
        {
            Value = (P.BoolMember && Tuning.*(P.BoolMember)) ? TEXT("true") : TEXT("false");
        }
        else
        {
            // SanitizeFloat trims trailing zeros down to one fractional digit, so
            // 900 prints as "900.0" and 0.05 as "0.05" — both paste back as valid
            // C++ float literals.
            Value = P.FloatMember ? FString::SanitizeFloat(Tuning.*(P.FloatMember)) : TEXT("0.0");
        }

        Json += FString::Printf(TEXT("%s\"%s\":%s"), (i == 0) ? TEXT("") : TEXT(","), P.Name, *Value);
    }
    Json += TEXT("}");

    return Json;
}

float OctoTuning::GetNormalized(const FOctoTuning& Tuning, const FOctoTuningParam& Param)
{
    if (Param.Kind == EOctoParamKind::Bool)
    {
        return (Param.BoolMember && Tuning.*(Param.BoolMember)) ? 1.f : 0.f;
    }

    if (!Param.FloatMember || Param.Max <= Param.Min)
    {
        return 0.f;
    }

    const float Value = Tuning.*(Param.FloatMember);
    return FMath::Clamp((Value - Param.Min) / (Param.Max - Param.Min), 0.f, 1.f);
}

void OctoTuning::SetNormalized(FOctoTuning& Tuning, const FOctoTuningParam& Param, float Alpha)
{
    const float T = FMath::Clamp(Alpha, 0.f, 1.f);

    if (Param.Kind == EOctoParamKind::Bool)
    {
        if (Param.BoolMember)
        {
            Tuning.*(Param.BoolMember) = (T >= 0.5f);
        }
        return;
    }

    if (!Param.FloatMember)
    {
        return;
    }

    // Snap to the nearest Step so a mouse drag lands on the same values the
    // Left/Right keys produce — otherwise the JSON dump fills up with noise
    // like 903.71741 that nobody wants to paste into a header.
    float Value = FMath::Lerp(Param.Min, Param.Max, T);
    if (Param.Step > 0.f)
    {
        Value = Param.Min + FMath::RoundToFloat((Value - Param.Min) / Param.Step) * Param.Step;
    }

    Tuning.*(Param.FloatMember) = FMath::Clamp(Value, Param.Min, Param.Max);
}

void OctoTuning::Nudge(FOctoTuning& Tuning, const FOctoTuningParam& Param, int32 Direction)
{
    if (Direction == 0)
    {
        return;
    }

    if (Param.Kind == EOctoParamKind::Bool)
    {
        if (Param.BoolMember)
        {
            // Left is always "off", Right always "on" — a toggle that depended on
            // the current value would make holding a direction flicker.
            Tuning.*(Param.BoolMember) = (Direction > 0);
        }
        return;
    }

    if (!Param.FloatMember)
    {
        return;
    }

    const float Value = Tuning.*(Param.FloatMember) + Param.Step * static_cast<float>(FMath::Sign(Direction));
    Tuning.*(Param.FloatMember) = FMath::Clamp(Value, Param.Min, Param.Max);
}

FString OctoTuning::FormatValue(const FOctoTuning& Tuning, const FOctoTuningParam& Param)
{
    if (Param.Kind == EOctoParamKind::Bool)
    {
        return (Param.BoolMember && Tuning.*(Param.BoolMember)) ? TEXT("On") : TEXT("Off");
    }

    if (!Param.FloatMember)
    {
        return FString();
    }

    const float Value = Tuning.*(Param.FloatMember);

    // Two decimals for the sub-unit knobs (grip, damping, friction), one for the
    // big ones — 900.00 is just noise.
    return (Param.Max <= 20.f)
        ? FString::Printf(TEXT("%.2f"), Value)
        : FString::Printf(TEXT("%.1f"), Value);
}

// ---- Persistence -----------------------------------------------------------

bool OctoTuning::SaveToIni(const FOctoTuning& Tuning, const FString& IniPath)
{
    if (IniPath.IsEmpty())
    {
        return false;
    }

    // Local, not GConfig — see the header for the two ways the global cache gets
    // this wrong.
    FConfigFile File;
    File.Read(IniPath); // preserves any unrelated sections a human added

    for (const FOctoTuningParam& Param : GetParams())
    {
        if (Param.Kind == EOctoParamKind::Bool)
        {
            if (Param.BoolMember)
            {
                File.SetBool(IniSection, Param.Name, Tuning.*(Param.BoolMember));
            }
        }
        else if (Param.FloatMember)
        {
            File.SetFloat(IniSection, Param.Name, Tuning.*(Param.FloatMember));
        }
    }

    // Explicit: FConfigFile::Write silently skips a non-dirty file AND returns
    // true, so a stale Dirty flag would look exactly like a successful save.
    File.Dirty = true;

    return File.Write(IniPath);
}

int32 OctoTuning::LoadFromIni(FOctoTuning& Tuning, const FString& IniPath, FString* OutOverridden)
{
    if (IniPath.IsEmpty() || !IFileManager::Get().FileExists(*IniPath))
    {
        return 0;
    }

    FConfigFile File;
    File.Read(IniPath);

    int32 NumOverridden = 0;

    for (const FOctoTuningParam& Param : GetParams())
    {
        FString Applied;

        if (Param.Kind == EOctoParamKind::Bool)
        {
            bool Value = false;
            if (!Param.BoolMember || !File.GetBool(IniSection, Param.Name, Value))
            {
                continue;
            }
            Tuning.*(Param.BoolMember) = Value;
            Applied = Value ? TEXT("true") : TEXT("false");
        }
        else
        {
            float Value = 0.f;
            if (!Param.FloatMember || !File.GetFloat(IniSection, Param.Name, Value))
            {
                continue;
            }
            Tuning.*(Param.FloatMember) = FMath::Clamp(Value, Param.Min, Param.Max);
            Applied = FString::SanitizeFloat(Tuning.*(Param.FloatMember));
        }

        NumOverridden++;
        if (OutOverridden)
        {
            *OutOverridden += FString::Printf(TEXT(" %s=%s"), Param.Name, *Applied);
        }
    }

    return NumOverridden;
}
