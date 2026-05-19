# Graphics Pipeline

This document defines the expected shape of the Toybox graphics/rendering pipeline.

The goal is to keep rendering code clean, backend-agnostic, easy to reason about, and strict about ownership. Each layer has a narrow responsibility and must not take ownership of work that belongs somewhere else.

## Shaders

Refer to the Shader Pipeline Docs [here](ShaderPipeline.md).

---

# Pipeline Shape

The Toybox graphics pipeline should follow this shape:

```txt
                Rendering
                    ↓
               FrameBuilder
                    ↓
            RenderPassPipeline
                    ↓
         RenderPassOperation(s)
           ↓                ↓
GraphicsResourceManager   IGraphicsBackend (Command endpoints)
           ↓                ↓
IGraphicsBackend (Resource endpoints)
           ↓                ↓
OpenGL / Vulkan / SDL GPU / Diligent / Other API
```

---

## Core Rules

- `Rendering` orchestrates the frame.
- `RenderFrameData` owns frame-scoped setup and cleanup.
- `RenderViewData` owns view-scoped setup and cleanup.
- `RenderPipeline` sequences render operations.
- `IRenderOperation` implements one focused rendering step.
- `GraphicsResourceManager` owns all graphics resource lifetime, caching, fallback, upload, update, and unload policy.
- `IGraphicsBackend` executes GPU commands and performs backend-native resource work when requested by the manager.

Graphics resources are manager-owned.  
GPU commands are operation-driven.  
Backend-native resources must not leak into high-level rendering APIs.

---

## Backend Split

`IGraphicsBackend` exposes two endpoint groups.

### Command Endpoints

Used by `Rendering` and `IRenderOperation`.

Examples:

```txt
Begin/end frame
Begin/end pass
Clear
Bind
Draw
Dispatch
Resolve/copy
Present
```

### Resource Endpoints

Used only by `GraphicsResourceManager`.

Examples:

```txt
Create resources
Update resources
Upload resource data
Destroy resources
```

Operations may command the GPU.  
Operations must not create, update, cache, unload, or destroy graphics resources directly.

---

## Responsibilities

### Rendering

Owns:

- Render loop orchestration
- Active `GraphicsResourceManager`
- Active `RenderPipeline`
- Frame lifecycle
- Surface lifecycle API
- High-level render failure handling

Does not:

- Implement render passes
- Manage graphics resources directly
- Own raw backend resources
- Call backend resource endpoints

### RenderTarget

Represents a renderable output target.

Examples:

```txt
Main window backbuffer
Editor viewport
Offscreen render target
Texture-backed output
Custom user surface
```

A `Surface` owns a stable Toybox id only.  
Its GPU backing resources are managed by `GraphicsResourceManager`.

### RenderFrameData

Owns frame-scoped data:

- Frame index and timing
- Global render settings
- View-independent scene inputs
- Shared lighting/shadow inputs
- Frame-scoped temporary resources

Does not execute rendering or own backend resources.

### RenderPipeline

Owns ordered `IRenderOperation`s.

It:

- Accepts prepared view data
- Runs operation preparation
- Runs operation execution
- Preserves operation order
- Wraps failures with operation debug info

It does not build frame/view data, draw directly, manage resources, or contain operation-specific conditionals.

### IRenderOperation

Owns one render step.

Examples:

```txt
Shadow pass
Depth prepass
Geometry pass
Lighting pass
Forward opaque pass
Transparent pass
Skybox
Post-process
Debug draw
UI
```

#### Prepare

CPU-side setup:

- Validate resources
- Build/sort draw lists
- Prepare material bindings
- Prepare uniforms
- Request uploads through `GraphicsResourceManager`

#### Execute

GPU command work:

- Bind
- Clear
- Draw
- Dispatch
- Resolve/copy

Execute must not call backend resource endpoints.

### GraphicsResourceManager

The only graphics resource authority.

Owns:

- Asset-backed resources
- Runtime resources
- Surface backing resources
- Buffers, textures, samplers, shaders, pipelines, render targets
- Upload/update/unload behavior
- Resource caching
- Usage tracking
- Stale resource unloading
- Fallback/default resources

It may call backend resource endpoints.

It does not issue draw calls, decide render order, execute pipelines, or own command policy.

### IGraphicsBackend

Owns:

- Backend device/context state
- Backend-native GPU objects
- Command execution
- Resource endpoint implementation
- Minimal API-specific state

It does not own high-level rendering policy, fallback policy, caching policy, or resource lifetime decisions.

---

## Resource Lifetime

All graphics resources are represented by stable Toybox ids, handles, or manager-owned records.

High-level systems may reference graphics resources, but never own their lifetimes.

### Asset-backed Resources

Managed by `GraphicsResourceManager`.

Examples:

```txt
Models
Meshes
Textures
Materials
Shaders
```

### Runtime Resources

Created from descriptions but still owned by `GraphicsResourceManager`.

Examples:

```txt
Buffers
Textures
Samplers
Pipelines
Render targets
Temporary pass resources
Surface backing resources
```

All updates and unloads must go through `GraphicsResourceManager`.

---

## Fallbacks

Fallbacks belong in `GraphicsResourceManager` or higher-level rendering policy, not in the backend.

Examples:

```txt
Default texture
Default normal texture
Default black texture
Fallback material
Fallback pipeline
Error resources
```

The backend should report failures, not silently substitute resources.

---

## Error Handling

Failures should use the engine’s standard result/error flow.

- `IGraphicsBackend` reports GPU/backend failures.
- `GraphicsResourceManager` decides whether to fall back or fail.
- `RenderPipeline` adds operation context.
- `Rendering` decides whether to skip or abort a frame.

Avoid silent fallback behavior in low-level systems.

---

## Extending the Pipeline

### Adding a Render Operation

Create a new `IRenderOperation` and register it with `RenderPipeline`.

Do not add large conditionals to `RenderPipeline`, backend-specific logic to `Rendering`, or resource ownership outside `GraphicsResourceManager`.

### Adding a Resource Type

Extend `GraphicsResourceManager`.

Do not expose raw backend handles or move lifetime logic into rendering, frame/view data, operations, or the backend.

### Adding a Backend

Implement `IGraphicsBackend`.

Do not change high-level rendering policy, resource lifetime policy, stale unloading, pass ordering, or pipeline ownership rules.

---

## Anti-Patterns

Avoid:

- Creating monolithic classes/files
- Letting the backend own resource policy
- Letting operations call backend resource endpoints
- Putting GPU command execution on `GraphicsResourceManager`
- Putting pass implementation details in `Rendering`
- Adding operation-specific conditionals to `RenderPipeline`
- Exposing raw backend resources upward

---

## Final Rule

If a change causes one layer to take on another layer’s responsibility, redesign it.