# Audio architecture

How sound works across the party games. The code is in `Game/Plugins/PartyAudio`
(runtime, game-agnostic); the content pipeline is `AI/audio_manifest.py` +
`AI/tools/prep_audio.py` + `AI/build_audio_assets.py`.

## The model: event → service → emitters

This is the Wwise/FMOD "sound event" model, built on native UE audio.

- **Gameplay never names a WAV.** It says *what happened, how hard, and where*
  (`UPartyAudioSubsystem::PlayEvent(Event, Location, Intensity, SourceKey)`).
- **`UPartySoundEvent`** (a data asset) decides *what that sounds like*:
  - which recording, picked from variants by intensity band, then no-repeat or shuffle-bag;
  - gain, pitch and low-pass cutoff as functions of intensity (soft hits are quieter *and* duller);
  - per-play jitter;
  - the per-source cooldown;
  - attenuation and concurrency.
- **Emitter components** turn physics into events, so most games need no audio code at all:

| Component | For | Drives intensity from |
|---|---|---|
| `UPartyImpactAudioComponent` | anything that collides | \|NormalImpulse\| / mass, normalised; two layers: self + surface hit |
| `UPartyLoopAudioComponent` | rolling, wind, engines | a 0..1 drive set every frame, with attack/release smoothing; the voice stops when silent |
| `APartyRandomSoundEmitter` | gulls, bells, gusts | random point in a box, random interval |

- **Mix routing** is on the SoundWaves (SoundClass per event, set at import):
  `SC_Master → SC_Music, SC_SFX → {Impacts, Movement, UI, Ambience, Stingers}`.
  Voice budgets are `CON_*` concurrency assets. Distance behaviour is `ATT_*`,
  which uses natural-sound falloff plus air-absorption low-pass.
- **Side-on listener.** `UPartyAudioSubsystem::SetAttenuationFocus(Component)`
  measures distance from the player character while panning still comes from
  the camera. Without it, the far-off camera attenuates everything. OctoOdyssey
  focuses on the octopus body.

The maths (selection, shaping, gating, smoothing) is the world-free
`PartyAudio::` namespace, tested by `PartyButtons.Audio.Framework.*`.

## Content pipeline

```
AI/audio_manifest.py          the only thing you edit
  │  prep_audio.py            source library → Assets/Audio/**.wav (+ CREDITS.md)
  │                           cut, mono, 48 kHz, high-pass, trim, fades / loop seam,
  │                           loudness-normalised per preset
  └─ build_audio_assets.py    (in editor) WAVs → SoundWaves, SC_/ATT_/CON_ mix assets,
                              SE_* events, PM_* physical materials on level materials
```

- **Levels.** Every file is normalised to its preset's loudness target, so no
  variant is hotter than its siblings. The mix is two knobs only: the event's
  `volume_db` and the SoundClass volumes.
- **Surfaces.** `DefaultEngine.ini` names SurfaceType1–4 Wood/Stone/Sand/Metal.
  `PM_*` physical materials go on the *render* materials, matched by keyword, so
  every mesh using a material reports its surface with no per-actor setup.
  Unmatched geometry plays the default (stone) layer.

## Adding sound to a new game

1. Add source files and events to `AI/audio_manifest.py`, then run both scripts.
2. Physics props: add a `UPartyImpactAudioComponent` and set `SelfEvent` and `SurfaceEvents`.
   If the body touches the world through **welded** child shapes (the octopus's
   arms), `AddTarget()` each one. A welded shape raises hits only if its own
   component asks for them, and Chaos delivers them to that child component, not the root.
3. Continuous sounds: add a `UPartyLoopAudioComponent` and call `SetDrive()` each tick.
4. One-shot moments: call `UPartyAudioSubsystem::Get(this)->PlayEvent(...)`.
5. For a side or top-down camera, call `SetAttenuationFocus` on the player character.

## OctoOdyssey hooks (`AOctoPawn`)

| Sound | Trigger | Intensity |
|---|---|---|
| Body impact (flesh + surface) | `ImpactAudio` on `BodySphere` + the 8 welded arm capsules | speed change, `AudioImpactMin/MaxSpeed` |
| Head impact | `ResolveHeadCollision` | head's inward speed × 0.7 |
| Arm plant | rising edge of an arm's blocked sweep | push-off deficit / ExtendSpeed |
| Push-off thump | plant stronger than `AudioPushOffThreshold` | rescaled above the threshold |
| Arm unstick | planted arm released, or pulled off by the launch | fixed |
| Roll loop | grounded (body hit within `AudioGroundedSeconds`) | \|ω_x\| × radius / `AudioRollMaxSpeed` |
| Air rush loop | always | speed between `AudioAirRushMin/MaxSpeed` |

All `Audio*` tunables are in the Tab dev menu (category Audio).

Debug with the console command `PartyAudio.Debug 1`, which labels every play in
the world with the event, intensity and variant.

Impact intensity subtracts the g·dt of speed that resting contact reports every
frame, so a body lying still doesn't "hit" harder at low frame rates.

## Not done yet

- **Phase 4:** UI and flow sounds: menu, checkpoint, death/splash, respawn, goal stinger, name entry. Shared UI slots on `APartyGameModeBase`.
- **Phase 5:** ambience and final mix:
  - sea/wind bed;
  - shoreline wave emitters;
  - gull scatterers;
  - a boat motor loop;
  - stinger ducking (`SM_StingerDuck`);
  - gain staging.
