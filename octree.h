#ifndef _OCTREE_H_
#define _OCTREE_H_

#define LEFT	0
#define RIGHT	4
#define BOTTOM	0
#define TOP	2
#define BACK	0
#define FRONT	1

#include "mesh.h"
#include "vec3.h"

typedef struct octree_node  octree_node;

typedef enum {
    STATUS_ACTIVE,
    STATUS_INACTIVE,
    STATUS_BOUNDARY
} octree_status;

struct octree_node {
    octree_status   status;		/* active, inactive, boundary	    */
    unsigned char   depth;		/* depth in tree (root has 0 depth) */
    char	    leaf;		/* whether or not node is leaf	    */

    int		    testid;

    int		    rep_vindex;		/* representative vertex index	    */
    vec3	    rep_vnormal;	/* representative vertex normal	    */

    vec3	    sp_center;		/* bounding sphere center ..	    */
    float	    sp_radius;		/*		    .. and radius   */

    vec3	    bb_midpt;		/* bounding box center ..	    */
    vec3	    bb_extent;		/*		    .. and extents  */

    vec3	    cone_normal;	/* normal cone direction ..	    */
    double	    cone_angle;		/*		    .. and angle    */

    int		   *activated;		/* activated[0]=# tris,		    */

    octree_node	   *parent;		/* pointer to parent node	    */
    octree_node	   *subtree[8];		/* pointers to children (x,y,z)	    */
};

typedef struct {
    mesh	 *mesh;			/* pointer to mesh		    */
    octree_node	 *root;			/* root of vertex octree	    */
    octree_node	**vertex_nodes;		/* per-vertex leaf node		    */
    octree_node	**activators;		/* per-triangle "activating" node   */
} octree;

octree *octree_create(mesh *m);
void	octree_free(octree *o);

#endif // !_OCTREE_H_
