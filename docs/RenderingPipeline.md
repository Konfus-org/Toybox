# Rendering Pipeline

The public rendering entry point remains `tbx::RenderingPipeline`. Its implementation is split
into private stages under `engine/src/systems/graphics/rendering_pipeline/` so the frame flow is
easy to follow without exposing a new extension API.

## Frame Flow

`RenderingPipeline::execute()` owns service lookup, frame begin/end, failure handling, and stage
ordering. The private stages run in this order:

1. `FrameUploadStage` extracts renderable World/entity data, resolves camera, light, material,
   mesh, texture, and sky inputs, then uploads per-frame GPU resources.
2. `VisibilityCullingStage` dispatches the GPU visibility work.
3. `GpuBarrierStage` applies the reusable GPU transition from visibility work to raster work.
4. `ShadowAtlasStage` renders shadow-casting batches into the shadow atlas.
5. `GpuBarrierStage` transitions shadow resources for gbuffer use.
6. `GBufferStage` submits visible renderable batches into the gbuffer.
7. `GpuBarrierStage` transitions gbuffer resources for lighting.
8. `LightingResolveStage` resolves lighting into the final HDR target.
9. `GpuBarrierStage` transitions the final target for presentation.
10. `PresentStage` draws the final frame texture and owns fallback failure-frame presentation.

## Private State

`RenderPipelineContext` owns GPU resource caches and frame-local render data shared by the private
stages. Stage classes are implementation details only; callers should continue using
`RenderingPipeline` and should not depend on stage headers or source layout.

The reusable `GpuBarrierStage` represents GPU resource/state transitions between render phases. It
does not perform GPU-to-CPU synchronization.
