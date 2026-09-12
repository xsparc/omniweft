# ADR-0003: Explicit hybrid representations

Status: accepted design baseline. Date: 2026-09-13.

## Decision

Use entities/components for world identity, immutable/versioned mesh assets, sparse 32-cubed block voxel chunks, typed image resources and frame-scoped view observations. Each representation has its own address and revision contract. The public API presents a consistent control pattern without pretending a mesh vertex, voxel and display pixel have identical semantics.

## Alternatives and consequences

A voxel-only engine simplifies one editable representation but restricts common mesh workflows and does not remove textures or screen pixels. A universal atom per rendered pixel is view-dependent and unsuitable for durable physical identity. An initial SDF/octree system adds smooth geometry/scale possibilities but increases meshing, collision and memory complexity before a baseline exists.

Hybrid data adds conversion/caching work. Conversions are explicit, versioned and potentially lossy. Static terrain and primitive/convex dynamic bodies bound collision complexity. Arbitrary deformable dynamic concave bodies remain outside the preview.

## Verification and revisit trigger

Mesh, texture and voxel examples prove exact addressed edits, stale-reference rejection and bounded derived work. PR-024/028 test chunk seams and collider publication. Measure chunk size/occupancy patterns before changing storage; add SDF/streaming backends through new contracts with error and memory evidence.
