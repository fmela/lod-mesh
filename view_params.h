#ifndef _VIEW_PARAMS_H_
#define _VIEW_PARAMS_H_

#include "vec3.h"

typedef struct {
    vec3    eye;	    /* Eye or camera location. */
    vec3    gaze;	    /* Gaze direction. */
    vec3    up;		    /* Up direction. */
    vec3    right;	    /* Right direction. */

    real    fovy;	    /* Field of view, in radians. */
    real    u;		    /* Umax or -Umin. */
    real    r;		    /* Rmax or -Rmin. */
    real    znear, zfar;    /* Near and far viewing planes. */

    vec3    tl;
    vec3    tr;
    vec3    br;
    vec3    bl;

    vec3    nr;		    /* Right view frustum normal. */
    vec3    nl;		    /* Left view frustum normal. */
    vec3    nt;		    /* Top view frustum normal. */
    vec3    nb;		    /* Bottom view frustum normal. */
} view_params;

void view_setup(view_params *vp,
		const vec3 eye, const vec3 gaze, const vec3 up,
		real fovy, real aspect, real znear, real zfar);
void view_matrix(const view_params *vp, real matrix[16]);

#endif // !_VIEW_PARAMS_H_
