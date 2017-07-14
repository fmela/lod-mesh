#ifndef _VEC3_H_
#define _VEC3_H_

#include <math.h>

typedef float real;
typedef real vec3[3];

#define VecZero(w)	    (w)[0]=(w)[1]=(w)[2]=0
#define VecSet(w,u)	    (w)[0]=(u)[0], (w)[1]=(u)[1], (w)[2]=(u)[2]

#define VecAdd(w,u,v)	    (w)[0]=(u)[0]+(v)[0], \
			    (w)[1]=(u)[1]+(v)[1], \
			    (w)[2]=(u)[2]+(v)[2]

#define VecSAdd(w,u,v,s)    (w)[0]=(u)[0]+(v)[0]*(s), \
			    (w)[1]=(u)[1]+(v)[1]*(s), \
			    (w)[2]=(u)[2]+(v)[2]*(s)

#define VecSub(w,u,v)	    (w)[0]=(u)[0]-(v)[0], \
			    (w)[1]=(u)[1]-(v)[1], \
			    (w)[2]=(u)[2]-(v)[2]

#define VecDot(u,v)	    (u)[0]*(v)[0]+(u)[1]*(v)[1]+(u)[2]*(v)[2]

#define VecScale(w,u,s)	    (w)[0]=(u)[0]*s, \
			    (w)[1]=(u)[1]*s, \
			    (w)[2]=(u)[2]*s

#define VecCross(w,u,v)	    (w)[0]=(u)[1]*(v)[2]-(u)[2]*(v)[1], \
			    (w)[1]=(u)[2]*(v)[0]-(u)[0]*(v)[2], \
			    (w)[2]=(u)[0]*(v)[1]-(u)[1]*(v)[0]

#define VecBlend(w,u,v,t)   (w)[0]=(1.0-(t))*(u)[0]+(t)*(v)[0], \
			    (w)[1]=(1.0-(t))*(u)[1]+(t)*(v)[1], \
			    (w)[2]=(1.0-(t))*(u)[2]+(t)*(v)[2]

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#define VecSwap(u,v) do { \
    real __t = (u)[0]; (u)[0] = (v)[0]; (v)[0] = __t; \
	 __t = (u)[1]; (u)[1] = (v)[1]; (v)[1] = __t; \
	 __t = (u)[2]; (u)[2] = (v)[2]; (v)[2] = __t; } while (0)

#define VecNormalize(v) do { \
    real __t = VecDot(v, v); \
    if (__t != 0.0) { \
	__t = 1. / sqrt(__t); \
	v[0] *= __t; \
	v[1] *= __t; \
	v[2] *= __t; \
    } } while (0)

#define Normal(n,v0,v1,v2) do { \
    vec3 __u, __v; \
    VecSub(__u, v1, v0); \
    VecSub(__v, v2, v0); \
    VecCross(n, __u, __v); } while (0)

#define DEG2RAD	(M_PI/180.0)
#define RAD2DEG	(180.0/M_PI)

#endif // !_VEC3_H_
