#ifndef _MESH_H_
#define _MESH_H_

#include <stdint.h>
#include "vec3.h"

typedef uint32_t    index3u[3];
typedef uint8_t	    color3ub[3];
typedef float	    texcoord2f[2];

typedef struct {
    uint32_t	nv;		/* number of vertices */
    vec3	*verts;		/* vertex array */
    vec3	*vnormals;	/* vertex normals */
    color3ub	*vcolors;	/* vertex colors */
    texcoord2f	*vtexcoords;	/* vertex texture coordinates */

    unsigned	nt;		/* number of triangles */
    index3u	*tris;		/* triangle vertex index array */
    vec3	*tnormals;	/* triangle normals */

    vec3	 min;		/* bounding box on vertices */
    vec3	 max;
} mesh;

mesh *mesh_load(const char *file);
void  mesh_free(mesh *m);
void  mesh_flip(mesh *m);

#endif // !_MESH_H_
