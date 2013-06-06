#ifndef _VFC_H_
#define _VFC_H_

#include "vec3.h"
#include "view_params.h"

int vf_point_inside(const view_params *vp, const vec3 v);
int vf_aabb_inside(const view_params *vp, const vec3 min, const vec3 max);
int vf_sphere_inside(const view_params *vp, const vec3 center, real radius);

#endif // !_VFC_H_
