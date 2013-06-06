#include "aabb.h"

#define TOL	1e-7

int
aabb_corners_pt_inside(const vec3 min, const vec3 max, const vec3 v)
{
    return (min[0]<=v[0]+TOL && v[0]-TOL<=max[0] &&
	    min[1]<=v[1]+TOL && v[1]-TOL<=max[1] &&
	    min[2]<=v[2]+TOL && v[2]-TOL<=max[2]);
}

int
aabb_midpt_pt_inside(const vec3 mid, const vec3 ext, const vec3 v)
{
    return (fabs(v[0]-mid[0])<=ext[0]+TOL &&
	    fabs(v[1]-mid[1])<=ext[1]+TOL &&
	    fabs(v[2]-mid[2])<=ext[2]+TOL);
}

int
aabb_corners_inside(const vec3 min, const vec3 max,
		    const vec3 pmin, const vec3 pmax)
{
    return (min[0]<=pmin[0] && pmax[0]<=max[0] &&
	    min[1]<=pmin[1] && pmax[1]<=max[1] &&
	    min[2]<=pmin[2] && pmax[2]<=max[2]);
}

void
aabb_corners_to_midpt(vec3 mid, vec3 ext, const vec3 min, const vec3 max)
{
    VecBlend(mid, min, max, 0.5);
    VecSub(ext, max, mid);
}

void
aabb_midpt_to_corners(vec3 min, vec3 max, const vec3 mid, const vec3 ext)
{
    VecSub(min, mid, ext);
    VecAdd(max, mid, ext);
}
