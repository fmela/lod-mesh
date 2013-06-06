#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "aabb.h"
#include "octree.h"
#include "vec3.h"
#include "vfc.h"
#include "view_params.h"
#include "timer.h"

static int
verify(real mid, int d, vec3 verts[], int lo, int hi, int code)
{
    int j, k;
    int fail = 0;
    const char *dims[3][2] =
	{{ "left", "right" }, { "bottom", "top" }, { "back", "front" }};

    for (j=lo; j<=hi; j++) {
	k=(verts[j][d]>=mid);
	if (k != code) {
	    printf("warning: incorrect at %d of [%d,%d] "
		   "is %s, should be %s\n",
		   j, lo, hi,
		   dims[d][k], dims[d][code]);
	    fail = 1;
	}
    }
    return fail;
}

/* Partition verts[lo] ... verts[hi] on dimension "dim" with d as discriminator.
 * All vertices with "dim" coordinate less than d will come before all vertices
 * with "dim" coordinate greater or equal to d. The routine will return an
 * index in the range [lo,hi+1] indicating where the partition falls. The array
 * will be rearranged so that "less-than" values fall in the range [lo,index-1]
 * and "greater-or-equal" values fall in the range [index,hi] */
static int
partition(vec3 verts[], int itable[], int lo, int hi, int dim, real d)
{
    int i, j, k;

    if (lo==hi) {
	if (verts[lo][dim]<d) return lo+1;
	else return lo;
    }

    i=lo; j=hi;
    for (;;) {
	while (i<j && verts[i][dim] <d) i++;
	while (j>i && verts[j][dim]>=d) j--;
	if (i==j) {
	    if (i==hi && verts[i][dim]<d) j=i+1;
	    break;
	}
	VecSwap(verts[i], verts[j]);
	k=itable[i]; itable[i]=itable[j]; itable[j]=k;
    }
    return j;
}

static octree_node *
build(int lo, int hi, unsigned char depth,
      vec3 verts[], int itable[], mesh *mesh)
{
    octree_node *o, *so;
    vec3 min, max, cen, span, d, dia1, dia2;
    real rad, rad2, p, q;
    int i, j, mini[3], maxi[3];
    int m, l, h, ll, lh, hl, hh; /* indices for partitioning */

    if (lo > hi) return NULL; /* shouldn't happen */

    /* sometimes some models have duplicate vertices :-( */
    while (hi>lo &&
	   verts[lo][0]==verts[hi][0] &&
	   verts[lo][1]==verts[hi][1] &&
	   verts[lo][2]==verts[hi][2])
	hi--;

    o = malloc(sizeof(*o));
    o->status = STATUS_INACTIVE;
    o->depth = depth;
    o->testid = 0; /* XXX */
    o->activated = NULL; /* XXX */

    if (lo == hi) {
	/* allocate and return singleton node */
	o->leaf = 1;
	for (j=0; j<8; j++) /* just to make sure.. */
	    o->subtree[j]=NULL;
	o->rep_vindex = itable[lo];
	VecSet(o->rep_vnormal, mesh->vnormals[itable[lo]]);
	VecSet(o->bb_midpt, verts[lo]);
	o->bb_extent[0] = o->bb_extent[1] = o->bb_extent[2] = 0;
	return o;
    }

    o->leaf = 0;

    /* find extent of vertices (want a "tight" octree_node), and also find
     * close to optimal bounding sphere by algorithm from graphics gems 1 */
    for (i=0; i<3; i++)
	mini[i] = maxi[i] = lo;
    for (j=lo+1; j<=hi; j++)
	for (i=0; i<3; i++) {
	    if (verts[mini[i]][i] > verts[j][i]) mini[i]=j; else
		if (verts[maxi[i]][i] < verts[j][i]) maxi[i]=j;
	}

    min[0] = verts[mini[0]][0];
    min[1] = verts[mini[1]][1];
    min[2] = verts[mini[2]][2];
    max[0] = verts[maxi[0]][0];
    max[1] = verts[maxi[1]][1];
    max[2] = verts[maxi[2]][2];
    aabb_corners_to_midpt(o->bb_midpt, o->bb_extent, min, max);

    for (i=0; i<3; i++) {
	VecSub(d, verts[maxi[i]], verts[mini[i]]);
	span[i]=VecDot(d, d);
    }
    if (span[0]>=span[1] && span[0]>=span[2]) {
	VecSet(dia1, verts[mini[0]]);
	VecSet(dia2, verts[maxi[0]]);
    } else if (span[1]>=span[0] && span[1]>=span[2]) {
	VecSet(dia1, verts[mini[1]]);
	VecSet(dia2, verts[maxi[1]]);
    } else {
	VecSet(dia1, verts[mini[2]]);
	VecSet(dia2, verts[maxi[2]]);
    }
    /* initial center */
    VecBlend(cen, dia1, dia2, 0.5);
    /* initial radius */
    VecSub(d, dia2, cen);
    rad = sqrt(rad2 = VecDot(d, d));

    for (j=lo; j<=hi; j++) {
	real d2;

	VecSub(d, verts[j], cen);
	d2 = VecDot(d, d);
	if (d2 > rad2) {
	    real p;
	    real d1 = sqrt(d2);
	    rad = (rad + d1) * 0.5;
	    rad2 = rad * rad;
	    p = d1 - rad;
	    cen[0] = (rad * cen[0] + p * verts[j][0]) / d1;
	    cen[1] = (rad * cen[1] + p * verts[j][1]) / d1;
	    cen[2] = (rad * cen[2] + p * verts[j][2]) / d1;
	}
    }

    VecSet(o->sp_center, cen);
    o->sp_radius = rad;

    /* set representative normal to closest to average of vertex normals */
    i=itable[lo];
    VecSet(d, mesh->vnormals[i]);
    for (j=lo+1; j<hi; j++) {
	i=itable[j];
	VecAdd(d, d, mesh->vnormals[i]);
    }
    VecNormalize(d);
    VecSet(o->rep_vnormal, d);

    /* choose representative vertex to be one with closest to representative
     * normal */
    p = VecDot(o->rep_vnormal, mesh->vnormals[itable[lo]]);
    m = lo;
    for (j=lo+1; j<=hi; j++) {
	q = VecDot(o->rep_vnormal, mesh->vnormals[itable[j]]);
	if (p < q) { p = q; m = j; }
    }
    o->rep_vindex = itable[m];

    VecZero(o->cone_normal);
    o->cone_angle = 1.0;

    /* x partition
     * |x-|x+|
     * lo m  hi
     *
     * y partition
     * |x-y-|x-y+|x+y-|x+y+|
     * lo   l    m    h    hi
     *
     * z partition
     * |x-y-z-|x-y-z+|x-y+z-|x-y+z+|x+y-z-|x+y-z+|x+y+z-|x+y+z+|
     * lo     ll     l      lh     m      hl     h      hh     hi
     */
    for (j=0; j<8; j++)
	o->subtree[j]=NULL;

    /* partition array on x */
    m = partition(verts, itable, lo, hi, 0, o->bb_midpt[0]);
    if (lo < m) {
	l = partition(verts, itable, lo, m-1, 1, o->bb_midpt[1]);
	if (lo < l) {
	    ll = partition(verts, itable, lo, l-1, 2, o->bb_midpt[2]);
	    if (lo < ll) {
		so = build(lo, ll-1, depth+1, verts, itable, mesh);
		so->parent = o;
		o->subtree[LEFT|BOTTOM|BACK] = so;
	    }
	    if (ll < l) {
		so = build(ll, l-1, depth+1, verts, itable, mesh);
		so->parent = o;
		o->subtree[LEFT|BOTTOM|FRONT] = so;
	    }
	}
	if (l < m) {
	    lh = partition(verts, itable, l, m-1, 2, o->bb_midpt[2]);
	    if (l < lh) {
		so = build(l, lh-1, depth+1, verts, itable, mesh);
		so->parent = o;
		o->subtree[LEFT|TOP|BACK] = so;
	    }
	    if (lh < m) {
		so = build(lh, m-1, depth+1, verts, itable, mesh);
		so->parent = o;
		o->subtree[LEFT|TOP|FRONT] = so;
	    }
	}
    }
    if (m <= hi) {
	h = partition(verts, itable, m,  hi, 1, o->bb_midpt[1]);
	if (m < h) {
	    hl = partition(verts, itable, m, h-1, 2, o->bb_midpt[2]);
	    if (m < hl) {
		so = build(m, hl-1, depth+1, verts, itable, mesh);
		so->parent = o;
		o->subtree[RIGHT|BOTTOM|BACK] = so;
	    }
	    if (hl < h) {
		so = build(hl, h-1, depth+1, verts, itable, mesh);
		so->parent = o;
		o->subtree[RIGHT|BOTTOM|FRONT] = so;
	    }
	}
	if (h <= hi) {
	    hh = partition(verts, itable, h,  hi, 2, o->bb_midpt[2]);
	    if (h < hh) {
		so = build(h, hh-1, depth+1, verts, itable, mesh);
		so->parent = o;
		o->subtree[RIGHT|TOP|BACK] = so;
	    }
	    if (hh <= hi) {
		so = build(hh, hi, depth+1, verts, itable, mesh);
		so->parent = o;
		o->subtree[RIGHT|TOP|FRONT] = so;
	    }
	}
    }

    return o;
}

static void
normalize_cones(octree_node *o)
{
    int k;
    if (o != NULL) {
	VecNormalize(o->cone_normal);
	if (!o->leaf)
	    for (k=0; k<8; k++)
		normalize_cones(o->subtree[k]);
    }
}

static void
fix_cones(octree_node *o)
{
    int k;
    if (o != NULL) {
	o->cone_angle = acos(o->cone_angle);
	if (!o->leaf)
	    for (k=0; k<8; k++)
		fix_cones(o->subtree[k]);
    }
}

octree *
octree_create(mesh *m)
{
    vec3 *vtmp;
    octree *tree;
    int j, k;
    octree_node *n;
    int *itable;
    double t;
    int wrong = 0;
    real angle;

#if 0
    int old, new;
    unsigned char uc;
    float tc;
#endif

    tree=malloc(sizeof(*tree));
    tree->mesh=m;

    printf("Constructing vertex octree... ");
    fflush(stdout);

    t=get_timer();
    /* use a copy of vertices so we dont mess up vertex indices in mesh */
    itable=malloc(sizeof(*itable)*m->nv);
    for (j=0; j<m->nv; j++)
	itable[j]=j;
    vtmp=malloc(sizeof(*vtmp)*m->nv);
    memcpy(vtmp, m->verts, sizeof(*vtmp)*m->nv);
    tree->root=build(0, m->nv-1, 0, vtmp, itable, m);
    tree->root->parent=NULL;
    tree->root->status=STATUS_BOUNDARY;
    free(itable);
    free(vtmp);

    t=get_timer()-t;
    printf("done [%gs]\n", t);

    printf("Associating vertices with nodes... ");
    fflush(stdout);

    t=get_timer();
    tree->vertex_nodes=malloc(sizeof(*tree->vertex_nodes)*m->nv);
    for (j=0; j<m->nv; j++) {
	n=tree->root;
	while (!n->leaf) {
	    if (!aabb_midpt_pt_inside(n->bb_midpt, n->bb_extent, m->verts[j])) {
		vec3 min, max;
		aabb_midpt_to_corners(min, max, n->bb_midpt, n->bb_extent);

		printf("warning!! not inside extent anymore (depth=%d)\n",
		       n->depth);
		printf("min={%g,%g,%g} max={%g,%g,%g} v={%g,%g,%g}\n",
		       min[0], min[1], min[2], max[0], max[1], max[2],
		       m->verts[j][0], m->verts[j][1], m->verts[j][2]);
	    }
	    k=0;
	    if (m->verts[j][0] >= n->bb_midpt[0]) k|=4;
	    if (m->verts[j][1] >= n->bb_midpt[1]) k|=2;
	    if (m->verts[j][2] >= n->bb_midpt[2]) k|=1;
	    if (n->subtree[k]==NULL) break;
	    n=n->subtree[k];
	}
	tree->vertex_nodes[j]=n;
	if (m->verts[n->rep_vindex][0] != m->verts[j][0] ||
	    m->verts[n->rep_vindex][1] != m->verts[j][1] ||
	    m->verts[n->rep_vindex][2] != m->verts[j][2]) {
	    printf("  orig vertex=(%g,%g,%g)\n",
		   m->verts[j][0], m->verts[j][1], m->verts[j][2]);
	    printf("faulty vertex=(%g,%g,%g) leaf=%d\n\n",
		   m->verts[n->rep_vindex][0], m->verts[n->rep_vindex][1],
		   m->verts[n->rep_vindex][2], n->leaf);
	    wrong++;
	}
    }
    t=get_timer()-t;
    if (t<0) t=0;
    printf("done [%gs]\n", t);

    printf("Finding triangle activators... ");
    fflush(stdout);
    t=get_timer();

    tree->activators=malloc(sizeof(*tree->activators)*m->nt);
    for (j=0; j<m->nt; j++) {
	int v0,v1,v2;
	int k0,k1,k2;

	v0=m->tris[j][0];
	v1=m->tris[j][1];
	v2=m->tris[j][2];
	n=tree->root;
	do {
	    k0=k1=k2=0;
	    if (m->verts[v0][0] >= n->bb_midpt[0]) k0|=4;
	    if (m->verts[v0][1] >= n->bb_midpt[1]) k0|=2;
	    if (m->verts[v0][2] >= n->bb_midpt[2]) k0|=1;

	    if (m->verts[v1][0] >= n->bb_midpt[0]) k1|=4;
	    if (m->verts[v1][1] >= n->bb_midpt[1]) k1|=2;
	    if (m->verts[v1][2] >= n->bb_midpt[2]) k1|=1;

	    if (m->verts[v2][0] >= n->bb_midpt[0]) k2|=4;
	    if (m->verts[v2][1] >= n->bb_midpt[1]) k2|=2;
	    if (m->verts[v2][2] >= n->bb_midpt[2]) k2|=1;

	    if (k0 != k1 || k0 != k2 || k1 != k2) break;
	    n=n->subtree[k0];
	} while (!n->leaf);

	if (k0 == k1) {
	    while (!n->leaf) {
		k0=k1=0;
		if (m->verts[v0][0] >= n->bb_midpt[0]) k0|=4;
		if (m->verts[v0][1] >= n->bb_midpt[1]) k0|=2;
		if (m->verts[v0][2] >= n->bb_midpt[2]) k0|=1;

		if (m->verts[v1][0] >= n->bb_midpt[0]) k1|=4;
		if (m->verts[v1][1] >= n->bb_midpt[1]) k1|=2;
		if (m->verts[v1][2] >= n->bb_midpt[2]) k1|=1;

		if (k0 != k1) break;
		n=n->subtree[k0];
	    }
	} else if (k0 == k2) {
	    while (!n->leaf) {
		k0=k2=0;
		if (m->verts[v0][0] >= n->bb_midpt[0]) k0|=4;
		if (m->verts[v0][1] >= n->bb_midpt[1]) k0|=2;
		if (m->verts[v0][2] >= n->bb_midpt[2]) k0|=1;

		if (m->verts[v2][0] >= n->bb_midpt[0]) k2|=4;
		if (m->verts[v2][1] >= n->bb_midpt[1]) k2|=2;
		if (m->verts[v2][2] >= n->bb_midpt[2]) k2|=1;

		if (k0 != k2) break;
		n=n->subtree[k0];
	    }
	} else if (k1 == k2) {
	    while (!n->leaf) {
		k1=k2=0;
		if (m->verts[v1][0] >= n->bb_midpt[0]) k1|=4;
		if (m->verts[v1][1] >= n->bb_midpt[1]) k1|=2;
		if (m->verts[v1][2] >= n->bb_midpt[2]) k1|=1;

		if (m->verts[v2][0] >= n->bb_midpt[0]) k2|=4;
		if (m->verts[v2][1] >= n->bb_midpt[1]) k2|=2;
		if (m->verts[v2][2] >= n->bb_midpt[2]) k2|=1;

		if (k1 != k2) break;
		n=n->subtree[k1];
	    }
	}
	tree->activators[j]=n;
	if (n->activated==NULL) {
	    n->activated=malloc(sizeof(*n->activated)*2);
	    n->activated[0]=1;
	    n->activated[1]=j;
	} else {
	    n->activated[0]+=1;
	    n->activated=realloc(n->activated,
				 sizeof(*n->activated)*(n->activated[0]+1));
	    n->activated[n->activated[0]]=j;
	}
    }

    t=get_timer()-t;
    printf("done [%gs]\n", t);

    printf("Computing normal cones... ");
    fflush(stdout);
    t=get_timer();

    /* for each triangle... */
    for (j=0; j<m->nt; j++) {
	n=tree->vertex_nodes[m->tris[j][0]];
	while (n) {
	    VecAdd(n->cone_normal, n->cone_normal, m->tnormals[j]);
	    n=n->parent;
	}
	n=tree->vertex_nodes[m->tris[j][1]];
	while (n) {
	    VecAdd(n->cone_normal, n->cone_normal, m->tnormals[j]);
	    n=n->parent;
	}
	n=tree->vertex_nodes[m->tris[j][2]];
	while (n) {
	    VecAdd(n->cone_normal, n->cone_normal, m->tnormals[j]);
	    n=n->parent;
	}
    }

    /* now go and normalize them all */
    normalize_cones(tree->root);

    /* now go back and compute the angles */
    for (j=0; j<m->nt; j++) {
	n=tree->vertex_nodes[m->tris[j][0]];
	while (n) {
	    angle=VecDot(n->cone_normal, m->tnormals[j]);
	    if (n->cone_angle > angle)
		n->cone_angle = angle;
	    n=n->parent;
	}
	n=tree->vertex_nodes[m->tris[j][1]];
	while (n) {
	    angle=VecDot(n->cone_normal, m->tnormals[j]);
	    if (n->cone_angle > angle)
		n->cone_angle = angle;
	    n=n->parent;
	}
	n=tree->vertex_nodes[m->tris[j][2]];
	while (n) {
	    angle=VecDot(n->cone_normal, m->tnormals[j]);
	    if (n->cone_angle > angle)
		n->cone_angle = angle;
	    n=n->parent;
	}
    }

    /* now convert to radians using acos */
    fix_cones(tree->root);

    t=get_timer()-t;
    printf("done [%gs]\n", t);

    return tree;
}

static void
free_node(octree_node *n)
{
    int k;

    if (n != NULL) {
	for (k=0; k<8; k++)
	    free_node(n->subtree[k]);
	free(n);
    }
}

void
octree_free(octree *o)
{
    free_node(o->root);
    free(o->vertex_nodes);
    free(o->activators);
    free(o);
}
