3D polygonal mesh renderer with dynamic level-of-detail (LOD).

An octree of the polygon mesh is computed and traversed for view-dependent
dynamic mesh simplification based on visibility (view frustum culling),
projected screen size, silhouette preservation heuristic, and hierarchical
back-face culling. Frame-coherent octree traversal is used to minimize
computation time.

Rendering is performed by OpenGL and accelerated through the use of vertex
buffer objects (VBO) to keep mesh geometry resident on the GPU.

This was my final project for ICS188 (Project in Advanced Computer Graphics),
Spring 2004, at UC Irvine, taught by Dr. Renato Pajarola.
