# Per-pixel lighting toggle — scaffold status

This branch (`feature/per-pixel-lighting`) adds a first pass at an opt-in, CVar-gated toggle
that moves N64 vertex lighting from the CPU (per-vertex, Gouraud-interpolated) to the GPU
(per-pixel), for the **OpenGL backend only**.

Toggle: `CVAR_PERPIXEL_LIGHTING` (`gPerPixelLighting` / `Setting.PerPixelLighting`), off by
default. No in-game UI checkbox has been added yet — set the CVar directly (e.g. via the
in-game console, or `CVarSetInteger(CVAR_PERPIXEL_LIGHTING, 1)` from an enhancement).

## What's implemented

- New shader-permutation bit `ShaderOpts::LIGHTING_PERPIXEL` (`interpreter.h`), threaded through
  `CCFeatures::opt_lighting_perpixel` and the shader-id building in `Interpreter::Flush()`'s
  caller.
- `LoadedVertex` gained a `normal[3]` field, populated in `GfxSpVertex` whenever `G_LIGHTING` is
  active (object-space, matching what the CPU path already dots against).
- When per-pixel mode is active for a draw, `GfxSpVertex` skips the CPU light-accumulation loop
  and leaves the vertex color white instead, so the color-combiner's SHADE input becomes a
  neutral pass-through.
- The OpenGL vertex packer pushes the extra normal floats; `default.shader.glsl` declares
  `aNormal`/`vNormal` and, in the fragment stage, computes `ambient + sum(max(dot(n, dir), 0) *
  color)` and multiplies it into `texel` right before fog is applied.
- Light data (ambient + up to `MAX_PERPIXEL_LIGHTS = 7` directional lights, already
  object-space/unit-vector from `Interpreter::CalculateNormalDir`) is uploaded once per
  `Flush()` via a new `GfxRenderingAPI::SetLightingUniforms()` virtual, default no-op so DX11/
  Metal keep compiling and behaving exactly as before.
- `IsPerPixelLightingActive()` checks the active backend by name and only returns true for
  `"OpenGL"`, so enabling the CVar under DX11/Metal is a no-op rather than a broken vertex
  layout.

## Known limitations / not done

1. **Not built or run.** No compiler was available in the environment this was written in.
   Expect at least minor build errors on a first attempt — go through it with a compiler and
   fix as you go before judging the approach.
2. **Positional lights (`G_LIGHTING_POSITIONAL`) still use the CPU path**, even with the CVar
   on. Point-light distance falloff needs world position threaded into the fragment shader too;
   scaffolded here as directional + ambient only.
3. **SHADE-input assumption**: this treats the vertex "shade" color as always multiplicative in
   the color-combiner formula, which is how the common cases behave but isn't universally true
   of every N64 combine mode. A fully faithful implementation would need to identify which
   `aInput`/`vInput` slot maps to `G_CCMUX_SHADE` per draw (via `ColorCombiner::
   shader_input_mapping`, which isn't reconstructible from the shader-id bits alone — see the
   `gfx_cc_get_features` comment trail) and substitute the per-pixel value there specifically,
   instead of a blanket final multiply.
4. **HLSL/Metal backends untouched** — DX11 and Metal builds should compile and run identically
   to before this branch; they simply don't support the toggle yet.
5. **Batch/flush race**: `Flush()` reads `mRsp->current_lights` at flush time, not at each
   buffered triangle's original vertex time. A light change that doesn't otherwise trigger an
   early flush (unlike alpha/fog/grayscale state changes, which already do) could apply the
   wrong lights to some buffered triangles. Fixing this means flushing on light-state change too.
6. **No settings-menu checkbox** — CVar only, no ImGui wiring yet.
7. Not run through `clang-format` / the repo's pre-commit hooks.

## Suggested next steps

Build it, fix compile errors, test on a simple large-flat-surface actor to confirm the effect
is genuinely per-pixel (smooth across the surface, not faceted), then decide whether the
SHADE-substitution simplification in point 3 is acceptable or needs the more precise
input-mapping-aware approach before shipping.
