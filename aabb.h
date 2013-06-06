#ifndef _BBOX_H_
#define _BBOX_H_

#include "vec3.h"

int aabb_corners_pt_inside(const vec3 min, const vec3 max, const vec3 pt);
int aabb_midpt_pt_inside(const vec3 mid, const vec3 ext, const vec3 pt);

int aabb_corners_inside(const vec3 min, const vec3 max,
			const vec3 pmin, const vec3 pmax);

void aabb_corners_to_midpt(vec3 mid, vec3 ext, const vec3 min, const vec3 max);
void aabb_midpt_to_corners(vec3 min, vec3 max, const vec3 mid, const vec3 ext);

#endif /* !_BBOX_H_ */
