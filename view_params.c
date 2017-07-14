#include "view_params.h"

void
view_setup(view_params *vp,
	   const vec3 eye, const vec3 gaze, const vec3 up,
	   real fovy, real aspect, real znear, real zfar)
{
    VecSet(vp->eye, eye);
    /* set up orthonormal gaze, up, and right vectors.
     * up is only required to be an approximation to the up direction */
    VecSet(vp->gaze, gaze);
    VecNormalize(vp->gaze);
    const real d = VecDot(vp->gaze, up);
    VecSAdd(vp->up, up, vp->gaze, -d);
    VecNormalize(vp->up);
    VecCross(vp->right, vp->gaze, vp->up);

    const real half_fov = 0.5 * fovy * DEG2RAD;
    vp->u = znear * tan(half_fov);
    vp->r = vp->u * aspect;
    vp->fovy = fovy;
    vp->znear = znear;
    vp->zfar = zfar;

    vp->tr[0] = vp->gaze[0]*vp->znear + vp->up[0]*vp->u + vp->right[0]*vp->r;
    vp->tr[1] = vp->gaze[1]*vp->znear + vp->up[1]*vp->u + vp->right[1]*vp->r;
    vp->tr[2] = vp->gaze[2]*vp->znear + vp->up[2]*vp->u + vp->right[2]*vp->r;

    vp->tl[0] = vp->gaze[0]*vp->znear + vp->up[0]*vp->u - vp->right[0]*vp->r;
    vp->tl[1] = vp->gaze[1]*vp->znear + vp->up[1]*vp->u - vp->right[1]*vp->r;
    vp->tl[2] = vp->gaze[2]*vp->znear + vp->up[2]*vp->u - vp->right[2]*vp->r;

    vp->br[0] = vp->gaze[0]*vp->znear - vp->up[0]*vp->u + vp->right[0]*vp->r;
    vp->br[1] = vp->gaze[1]*vp->znear - vp->up[1]*vp->u + vp->right[1]*vp->r;
    vp->br[2] = vp->gaze[2]*vp->znear - vp->up[2]*vp->u + vp->right[2]*vp->r;

    vp->bl[0] = vp->gaze[0]*vp->znear - vp->up[0]*vp->u - vp->right[0]*vp->r;
    vp->bl[1] = vp->gaze[1]*vp->znear - vp->up[1]*vp->u - vp->right[1]*vp->r;
    vp->bl[2] = vp->gaze[2]*vp->znear - vp->up[2]*vp->u - vp->right[2]*vp->r;

    VecCross(vp->nr, vp->br, vp->tr);
    VecNormalize(vp->nr);

    VecCross(vp->nl, vp->tl, vp->bl);
    VecNormalize(vp->nl);

    VecCross(vp->nt, vp->tr, vp->tl);
    VecNormalize(vp->nt);

    VecCross(vp->nb, vp->bl, vp->br);
    VecNormalize(vp->nb);
}

void
view_matrix(const view_params *vp, real matrix[16])
{
#define M(i,j)	matrix[4*j+i]
    M(0,0) = vp->right[0];
    M(0,1) = vp->right[1];
    M(0,2) = vp->right[2];
    M(0,3) = 0;

    M(1,0) = vp->up[0];
    M(1,1) = vp->up[1];
    M(1,2) = vp->up[2];
    M(1,3) = 0;

    M(2,0) = -vp->gaze[0];
    M(2,1) = -vp->gaze[1];
    M(2,2) = -vp->gaze[2];
    M(2,3) = 0;

    M(3,0) = 0;
    M(3,1) = 0;
    M(3,2) = 0;
    M(3,3) = 1;
}
