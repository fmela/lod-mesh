#include "vfc.h"

int
vf_point_inside(const view_params *vp, const vec3 v)
{
    vec3 v_eye;

    /* transform vertex to eye space */
    VecSub(v_eye, v, vp->eye);

    /* check if it's inside near and far z planes */
    real d = VecDot(vp->gaze, v_eye);
    if (d < vp->znear || d > vp->zfar)
	return 0;

    /* now if it's outside with respect to any of top, bottom, left or right
     * view planes, it's outside the vf, otherwise it's inside */
    if (VecDot(v_eye, vp->nr) > 0) return 0;
    if (VecDot(v_eye, vp->nl) > 0) return 0;
    if (VecDot(v_eye, vp->nt) > 0) return 0;
    if (VecDot(v_eye, vp->nb) > 0) return 0;
    return 1;
}

int
vf_aabb_inside(const view_params *vp, const vec3 min, const vec3 max)
{
    vec3 tmp;

    if (vf_point_inside(vp, min)) return 1;

    if (vf_point_inside(vp, max)) return 1;

    tmp[0]=min[0]; tmp[1]=max[1]; tmp[2]=max[2];
    if (vf_point_inside(vp, tmp)) return 1;

    tmp[0]=min[0]; tmp[1]=min[1]; tmp[2]=max[2];
    if (vf_point_inside(vp, tmp)) return 1;

    tmp[0]=min[0]; tmp[1]=max[1]; tmp[2]=min[2];
    if (vf_point_inside(vp, tmp)) return 1;

    tmp[0]=max[0]; tmp[1]=min[1]; tmp[2]=max[2];
    if (vf_point_inside(vp, tmp)) return 1;

    tmp[0]=max[0]; tmp[1]=max[1]; tmp[2]=min[2];
    if (vf_point_inside(vp, tmp)) return 1;

    tmp[0]=max[0]; tmp[1]=min[1]; tmp[2]=min[2];
    if (vf_point_inside(vp, tmp)) return 1;

    return 0;
}

int
vf_sphere_inside(const view_params *vp, const vec3 center, real radius)
{
    vec3 c_eye;

    /* transform center to eye space */
    VecSub(c_eye, center, vp->eye);

    /* check if it's inside near and far z planes */
    real d = VecDot(vp->gaze, c_eye);
    if (d+radius < vp->znear || d-radius > vp->zfar) return 0;

    /* now if it's outside with respect to any of top, bottom, left or right
     * view planes, it's outside the vf, otherwise it's inside */
    if (VecDot(c_eye, vp->nr) > radius) return 0;
    if (VecDot(c_eye, vp->nl) > radius) return 0;
    if (VecDot(c_eye, vp->nt) > radius) return 0;
    if (VecDot(c_eye, vp->nb) > radius) return 0;
    return 1;
}
