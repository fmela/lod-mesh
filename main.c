/* TODO:
 * Change code so that octree bounding sphere is distance from center to
 * farthest vertex of any triangle which has a vertex inside the octree node */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#if defined(__APPLE__)
# include <OpenGL/gl.h>
# include <GLUT/glut.h>
#else
# include <GL/gl.h>
# include <GL/glut.h>
# include <GL/glext.h>
# include <GL/freeglut_ext.h>
#endif

#include "aabb.h"
#include "draw_string.h"
#include "octree.h"
#include "mesh.h"
#include "view_params.h"
#include "vec3.h"
#include "vfc.h"
#include "shader.h"
#include "timer.h"

int		win_width = 640*2;
int		win_height = 480*2;

real		view_theta = 0.0;
real		view_phi = 0.0;
real		eye_dist = 3.0;
view_params	view_info;

int		lock = 0;
int		fullres = 1;
int		draw_octree = 0;
int		help = 0;
int		wire = 0;
int		bf_cull = 1;
int		top_view = 0;
int		mouse_state = 0;
int		mouse_x, mouse_y;

int		pixel_shade = 0;
shader*		shader_strings = NULL;
GLuint		shader_vert_program = 0;
GLuint		shader_frag_program = 0;
GLuint		shader_program = 0;

int		vbo_init = 0;
GLuint		vbo_id[3];

mesh*		m;
octree*		tree;

int*		tri_index=NULL;
octree_node**	proxies=NULL;

/* active lists point to boundary octree nodes */
typedef struct active_node active_node;
struct active_node {
    octree_node	*node;
    active_node	*next;
} *active_list;

int		current_testid = 0;
int		num_tests = 0;
int		num_saved = 0;

void		spherical(real v[3], real r, real theta, real phi);
void		mouse_button(int button, int state, int x, int y);
void		mouse_motion(int x, int y);
void		display(void);
void		reshape(int w, int h);
void		key_press(unsigned char c, int x, int y);
void		render_octree(const octree_node *o);
void		lod_render(int update, const view_params *vp,
			   int *collapsed, int *culled, int *rendered);
void		fullres_render();
int		tree_depth(const octree_node *o);

float		detail_threshold = 1e-9;
float		silhouette_threshold = 5e-10;

static void
print_info(GLuint program)
{
    int len = 0;

    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

    if (len != 0) {
	char *buf = malloc(len);
	glGetShaderInfoLog(program, len, &len, buf);
	printf("Program Info Log:\n%s\n", buf);
	free(buf);
    } else {
	printf("No Info Log\n");
    }
}

void
init_shader()
{
    GLint status;

    /* if (!LoadGLFunc(glCreateShaderObject) ||
	!LoadGLFunc(glShaderSource) ||
	!LoadGLFunc(glCompileShader) ||
	!LoadGLFunc(glGetObjectParameteriv) ||
	!LoadGLFunc(glAttachObject) ||
	!LoadGLFunc(glLinkProgram) ||
	!LoadGLFunc(glCreateProgramObject) ||
	!LoadGLFunc(glUseProgramObject) ||
	!LoadGLFunc(glDetachObject) ||
	!LoadGLFunc(glDeleteObject) ||
	!LoadGLFunc(glGetInfoLog) ||
	!LoadGLFunc(glGetUniformLocation) ||
	!LoadGLFunc(glUniform3f)) {
	printf("GLSL not supported. Sorry!\n");
	exit(1);
    } */

    shader_strings = shader_load("shader");
    if (shader_strings == NULL) {
	printf("Could not load ``simple-shader'' source.\n");
	exit(1);
    }

    shader_vert_program = glCreateShader(GL_VERTEX_SHADER);
    shader_frag_program = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(shader_vert_program, 1,
		   (const GLchar **)&shader_strings->vshader, NULL);
    glShaderSource(shader_frag_program, 1,
		   (const GLchar **)&shader_strings->fshader, NULL);

    glCompileShader(shader_vert_program);
    glGetProgramiv(shader_vert_program, GL_COMPILE_STATUS, &status);
    print_info(shader_vert_program);
    if (!status) {
	printf("vertex program compilation failed\n");
	exit(1);
    }

    glCompileShader(shader_frag_program);
    glGetProgramiv(shader_frag_program, GL_COMPILE_STATUS, &status);
    print_info(shader_frag_program);
    if (!status) {
	printf("fragment program compilation failed\n");
	exit(1);
    }

    shader_program = glCreateProgram();
    glAttachShader(shader_program, shader_vert_program);
    glAttachShader(shader_program, shader_frag_program);

    glLinkProgram(shader_program);

    glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
    print_info(shader_program);
    if (!status) {
	printf("program link failed\n");
	exit(1);
    }
}

void
delete_shader()
{
    int k;

    printf("deleting shader... \n");

    glUseProgram(0);

    glDetachShader(shader_program, shader_vert_program);
    if ((k=glGetError()) != 0) printf("error0=%x\n", k);
    glDeleteShader(shader_vert_program);
    if ((k=glGetError()) != 0) printf("error1=%x\n", k);

    glDetachShader(shader_program, shader_frag_program);
    if ((k=glGetError()) != 0) printf("error2=%x\n", k);
    glDeleteShader(shader_frag_program);
    if ((k=glGetError()) != 0) printf("error3=%x\n", k);

    glDeleteProgram(shader_program);
    if ((k=glGetError()) != 0) printf("error4=%x\n", k);
}

void
create_vbos()
{
    if (!vbo_init) {
	/*
	if (glGenBuffers == NULL) {
	    if (!LoadGLFunc(glGenBuffers) ||
		!LoadGLFunc(glBindBuffer) ||
		!LoadGLFunc(glBufferData) ||
		!LoadGLFunc(glDeleteBuffers)) {
		printf("vertex_buffer_object extension not supported!\n");
		exit(1);
	    }
	    printf("vertex_buffer_object extension loaded\n");
	}
	*/

	glGenBuffers(3, vbo_id);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_id[0]);
	glBufferData(GL_ARRAY_BUFFER, m->nv*sizeof(vec3),
			m->verts, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_id[1]);
	glBufferData(GL_ARRAY_BUFFER, m->nv*sizeof(vec3),
			m->vnormals, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_id[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,  m->nt*sizeof(index3u),
			m->tris, GL_STATIC_DRAW);

	vbo_init = 1;

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

void
delete_vbos()
{
    if (vbo_init) {
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id[0]);
	glBufferData(GL_ARRAY_BUFFER, 0, NULL, 0);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_id[1]);
	glBufferData(GL_ARRAY_BUFFER, 0, NULL, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_id[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, NULL, 0);

	glDeleteBuffers(3, vbo_id);
	vbo_init = 0;
    }
}

static int
test_silhouette(const octree_node *n, const view_params *vp)
{
    vec3 n_view, n_top;
    real view_angle, theta;

    /* find view cone and angle */
    VecSub(n_view, n->sp_center, vp->eye);
    VecNormalize(n_view);

    VecSAdd(n_top, n->sp_center, vp->up, n->sp_radius);
    VecSub(n_top, n_top, vp->eye);
    VecNormalize(n_top);

    view_angle = acos(VecDot(n_view, n_top));

    theta = acos(VecDot(n_view, n->cone_normal));
    if (theta - (n->cone_angle + view_angle) > M_PI*.5)
	return 1;	/* front facing */
    if (theta + (n->cone_angle + view_angle) < M_PI*.5)
	return -1;	/* back facing */
    return 0;		/* (possibly) on silhouette */
}

static real
screen_area(const octree_node *n, const view_params *vp)
{
    /* size of sphere on screen is pi*r^2*f^2/d^2, where
     * r = radius of sphere
     * f = distance to view plane
     * d = distance of center of sphere to view plane */

    /* assuming eye is at origin, equation of image plane in gaze direction is
     * G_x * x + G_y * y + G_z * z - D
     * where G is the gaze direction vector and D is the view plane distance
     * so we first subtract eye position from origin of sphere, and then
     * compute it's project area. also check to see if sphere is entirely
     * behind near plane or entire past far plane */
    real s[3];
    real d;

    VecSub(s, n->sp_center, vp->eye);
    d = VecDot(vp->gaze, s);
    if (d + n->sp_radius <= vp->znear || d - n->sp_radius >= vp->zfar)
	/* sphere entirely behind or in front of viewing plane */
	return 0.0;
    d -= vp->znear;
    return M_PI * (n->sp_radius*n->sp_radius) * (vp->znear*vp->znear) / (d*d);
}

static int
test_node(octree_node *n, const view_params *vp)
{
    real threshold;
    int k;

    if (n->testid == current_testid) {
	num_saved++;
	return n->status == STATUS_ACTIVE;
    }

    n->testid = current_testid;
    num_tests++;

    if (!vf_sphere_inside(vp, n->sp_center, n->sp_radius))
	return 0;
    k = test_silhouette(n, vp);
    if (k < 0)
	return 0;
    threshold = (k == 0 ? silhouette_threshold : detail_threshold);
    return screen_area(n, vp) >= threshold;
}

void
mark_inactive(octree_node *n)
{
    int k;

    if (n->status != STATUS_INACTIVE) {
	n->status = STATUS_INACTIVE;
	for (k=0; k<8; k++)
	    if (n->subtree[k])
		mark_inactive(n->subtree[k]);
    }
}

/* nodes on boundary are those whose parents are determined to be expanded but
 * are not determined to be needing expansion themselves. to update the list,
 * we look at every node on the boundary. if it needs to be expanded, take it
 * off the list, and put it's children on the list, and come back to the
 * children. */
void
update_active_list(const view_params *vp)
{
    active_node *n,*p,*a;
    octree_node *o;
    int k;
    int allocs = 0;
    int frees = 0;

    /* advance current test id no */
    current_testid++;

    /* if active_list is NULL, this is being run for the first time, so
     * allocate a node pointing to root of tree */
    if (active_list == NULL) {
	active_list = malloc(sizeof(*active_list));
	active_list->node = tree->root;
	active_list->node->status = STATUS_BOUNDARY;
	active_list->next = NULL;
    }

    num_saved = 0;
    num_tests = 0;

    p = NULL;
    n = active_list;
    while (n != NULL) {
	o = n->node;
	if (o->status != STATUS_BOUNDARY) {
	    if (o->status != STATUS_INACTIVE)
		printf("WARNING: %s node on active list!\n",
		       o->status == STATUS_ACTIVE ? "active" : "unknown");
	    /* delete it from list */
	    if (p) {
		p->next = n->next;
		free(n);
		n = p->next;
	    } else {
		active_list = n->next;
		free(n);
		n = active_list;
	    }
	    frees++;
	    continue;
	}

	/* check if this boundary node should be expanded. */
	if (o->testid == current_testid) {
	    p = n;
	    n = n->next;
	    num_saved++;
	    continue;
	}

	if (!o->leaf && test_node(o, vp)) {
	    /* mark it active, take it out of the list, and insert it's
	     * children, keeping them in order */
	    o->status = STATUS_ACTIVE;
	    for (k=7; k>=0; k--)
		if (o->subtree[k]) {
		    /* increment both, but be lazy about the real work ;-) */
		    allocs++; frees++;
		    o->subtree[k]->status = STATUS_BOUNDARY;
		    n->node = o->subtree[k];
		    k--;
		    break;
		}
	    for (; k>=0; k--)
		if (o->subtree[k]) {
		    allocs++;
		    a = malloc(sizeof(*a));
		    o->subtree[k]->status = STATUS_BOUNDARY;
		    a->node = o->subtree[k];
		    a->next = n->next;
		    n->next = a;
		}
	    continue;
	}
	while (o->parent && !test_node(o->parent, vp))
	    o = o->parent;
	if (n->node != o) {
	    n->node = o;
	    if (o->parent)
		o->parent->testid = current_testid;

	    /* now we've come to a node which was active before and now needs
	     * to be put on the boundary. mark all nodes below this one
	     * inactive. this will ensure they get deleted from the active list
	     * by the code at the beginning of the for loop. */
	    o->status = STATUS_BOUNDARY;
	    for (k=0; k<8; k++)
		if (o->subtree[k])
		    mark_inactive(o->subtree[k]);
	}
	/* advance to next node */
	p = n;
	n = n->next;
    }

//  printf("%d tests, %d remembered, %d allocated, %d freed\n",
//	   num_tests, num_saved, allocs, frees);
}

int
main(int argc, char **argv)
{
    int j;
    vec3 mid;
    double s, scale, t;

    if (argc != 2) {
	fprintf(stderr, "usage: %s [PLY file]\n", argv[0]);
	exit(1);
    }

    printf("Loading mesh... ");
    fflush(stdout);

    t=get_timer();
    m=mesh_load(argv[1]);
    t=get_timer()-t;
    if (m==NULL) {
	fprintf(stderr, "error loading mesh\n");
	exit(1);
    }
    printf("done [%gs]\n", t);

    scale = 0;
    for (j=0; j<3; j++) {
	s = m->max[j] - m->min[j];
	if (scale < s) scale = s;
    }
    scale = 2.0 / scale;
    VecBlend(mid, m->min, m->max, 0.5);
    VecSub(m->min, m->min, mid);
    VecSub(m->max, m->max, mid);
    VecScale(m->min, m->min, scale);
    VecScale(m->max, m->max, scale);
    for (j=0; j<m->nv; j++) {
	VecSub(m->verts[j], m->verts[j], mid);
	VecScale(m->verts[j], m->verts[j], scale);
    }

    tree=octree_create(m);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(win_width, win_height);
    glutCreateWindow("ICS188 Final Project - "
		     "Hierarchical Dynamic Simplification");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse_button);
    glutMotionFunc(mouse_motion);
    glutKeyboardFunc(key_press);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glShadeModel(GL_SMOOTH);

    create_vbos();

    glutMainLoop();

    return 0;
}

void
spherical(real v[3], real r, real theta, real phi)
{
    theta *= DEG2RAD; phi *= DEG2RAD;

    v[0] = r * cos(theta) * cos(phi);
    v[1] = r * sin(phi);
    v[2] = r * sin(theta) * cos(phi);
}

void
mouse_button(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON) { /* rotation */
	if (state == GLUT_DOWN && mouse_state == 0) {
	    mouse_state = 1;
	    mouse_x = x;
	    mouse_y = y;
	} else if (state == GLUT_UP) {
	    mouse_state = 0;
	}
    } else if (button == GLUT_RIGHT_BUTTON) { /* zoom */
	if (state == GLUT_DOWN && mouse_state == 0) {
	    mouse_state = 2;
	    mouse_y = y;
	} else if (state == GLUT_UP) {
	    mouse_state = 0;
	}
    }
}

void
mouse_motion(int x, int y)
{
    if (mouse_state == 1) {
	view_theta += (x - mouse_x) / 5.0;
	view_phi += (y - mouse_y) / 5.0;
	mouse_x = x;
	mouse_y = y;

	if (view_theta >= 360.0) view_theta -= 360.0;
	else if (view_theta < 0.0) view_theta += 360.0;

	if (view_phi >= 360.0) view_phi -= 360.0;
	else if (view_phi < 0.0) view_phi += 360.0;

	glutPostRedisplay();
    } else if (mouse_state == 2) {
	eye_dist -= (y - mouse_y) * .02;
	mouse_y = y;
	if (eye_dist > 50.0) eye_dist = 50.0;
	else if (eye_dist < .1) eye_dist = .1;

	glutPostRedisplay();
    }
}

void
setup_lighting()
{
    float	light_pos[] = { 0.0, 1.0, 1.0, 1.0 };
//  float	light_dir[] = { 0.0, -1.0, 0.0, 1.0 };
//  float	light_exp[] = { 32 };
//  float	light_cutoff[] = { 90 };

    float	light_ambient[] = { 0.0, 0.0, 0.0, 1.0 };
    float	light_color[] = { 0.6, 0.1, 0.1, 1.0 };

    float	white_color[] = { 1.0, 1.0, 1.0, 1.0 };
    float	black_color[] = { 0.0, 0.0, 0.0, 1.0 };

    float	mat_diffuse[] = { 0.7, 0.7, 0.7, 1.0 };
    float	mat_specular[] = { 0.5, 0.5, 0.5, 1.0 };
    float	mat_shininess = 16.0;

    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, black_color);
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
//  glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, light_dir);
//  glLightfv(GL_LIGHT0, GL_SPOT_EXPONENT, light_exp);
//  glLightfv(GL_LIGHT0, GL_SPOT_CUTOFF, light_cutoff);

    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_color);
    glLightfv(GL_LIGHT0, GL_SPECULAR, white_color);

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
}

void
display(void)
{
    vec3 eye;
    vec3 gaze;
    vec3 up;
    char buf[512];
    int coll, cull, rend;
    int update = !lock;

    /* XXX shader load */
    if (pixel_shade) {
	if (shader_program == 0)
	    init_shader();
    } else {
	if (shader_program != 0) {
	    delete_shader();
	    printf("deleted shader\n");
	    shader_program = 0;
	    shader_vert_program = 0;
	    shader_frag_program = 0;
	}
    }

    /* compute eye position */
    spherical(eye, eye_dist, view_theta, view_phi);
    VecScale(gaze, eye, -1);
    up[0] = up[2] = 0;
    up[1] = (view_phi >= 90 && view_phi <= 270) ? -1 : +1;

    if (top_view) {
	vec3 t_eye = { 0, 2, 0 };
	vec3 t_gaze = { 0, -1, 0 };
	vec3 t_up = { 0, 0, 1 };

	view_setup(&view_info, t_eye, t_gaze, t_up,
		   45.0, (real)win_width / (real)win_height, .01, 100.0);
    } else {
	view_setup(&view_info, eye, gaze, up,
		   45.0, (real)win_width / (real)win_height, .01, 100.0);
    }


//  if (shader_program != 0)
//	glUniform3f(glGetUniformLocation(shader_program, "EyePos"),
//		    eye[0], eye[1], eye[2]);

    /* setup viewing parameters */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-view_info.r, view_info.r,
	      -view_info.u, view_info.u,
	      view_info.znear, view_info.zfar);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    setup_lighting();

    {
	real m[16];
	view_matrix(&view_info, m);
	glMultMatrixf(m);
	glTranslatef(-view_info.eye[0], -view_info.eye[1], -view_info.eye[2]);
    }

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (top_view)
	view_setup(&view_info, eye, gaze, up,
		   45.0, (real)win_width / (real)win_height, .01, 100.0);

    if (bf_cull && !draw_octree && !top_view)
	glEnable(GL_CULL_FACE);
    else
	glDisable(GL_CULL_FACE);

    if (wire && !draw_octree) {
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glColor3f(0, 0, 0);
	if (fullres) {
	    fullres_render();
	} else {
	    lod_render(update, &view_info, &coll, &cull, &rend);
	    update = 0;
	}
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(0.5, 1.0);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    if (wire != 2 || draw_octree) {
	if (draw_octree) {
	    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	    glDisable(GL_LIGHTING);
	    update_active_list(&view_info);
	    render_octree(tree->root);
	    coll=cull=rend=0;
	} else {
	    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	    if (pixel_shade)
		glUseProgram(shader_program);
	    if (fullres) {
		fullres_render();
		coll=cull=0;
		rend=m->nt;
	    } else {
		lod_render(update, &view_info, &coll, &cull, &rend);
	    }
	    if (pixel_shade)
		glUseProgram(0);
	}
    } else {
	coll=cull=rend=0;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_POLYGON_OFFSET_FILL);

    if (top_view) {
	vec3 ntl, ftl, ntr, ftr, nbl, fbl, nbr, fbr;

	VecSet(ftl, view_info.tl);
	VecNormalize(ftl); VecScale(ntl, ftl, view_info.znear);
	VecAdd(ftl, ftl, eye); VecAdd(ntl, ntl, eye);

	VecSet(ftr, view_info.tr);
	VecNormalize(ftr); VecScale(ntr, ftr, view_info.znear);
	VecAdd(ftr, ftr, eye); VecAdd(ntr, ntr, eye);

	VecSet(fbl, view_info.bl);
	VecNormalize(fbl); VecScale(nbl, fbl, view_info.znear);
	VecAdd(fbl, fbl, eye); VecAdd(nbl, nbl, eye);

	VecSet(fbr, view_info.br);
	VecNormalize(fbr); VecScale(nbr, fbr, view_info.znear);
	VecAdd(fbr, fbr, eye); VecAdd(nbr, nbr, eye);

	glColor3f(0.0, 0.5, 0.0);

	glBegin(GL_QUADS);
	    /* one quad for image plane */
	    glVertex3fv(ntr);
	    glVertex3fv(ntl);
	    glVertex3fv(nbl);
	    glVertex3fv(nbr);

	    /* one quad for "far" plane */
	    glVertex3fv(ftr);
	    glVertex3fv(ftl);
	    glVertex3fv(fbl);
	    glVertex3fv(fbr);

	    /* top plane */
	    glVertex3fv(ntr);
	    glVertex3fv(ftr);
	    glVertex3fv(ftl);
	    glVertex3fv(ntl);

	    /* left plane */
	    glVertex3fv(ntl);
	    glVertex3fv(ftl);
	    glVertex3fv(fbl);
	    glVertex3fv(nbl);

	    /* bottom plane */
	    glVertex3fv(nbl);
	    glVertex3fv(fbl);
	    glVertex3fv(fbr);
	    glVertex3fv(nbr);

	    /* right plane */
	    glVertex3fv(nbr);
	    glVertex3fv(fbr);
	    glVertex3fv(ftr);
	    glVertex3fv(ntr);
	glEnd();
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, win_width, 0, win_height, -1, +1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_XOR);

    if (!draw_octree) {
	sprintf(buf,
		"COLLAPSED %d VIEW-FRUSTUM CULLED %d RENDERED %d [%d%%]",
		coll, cull, rend, (int)(100 * (double)rend / (double)m->nt));
	glRasterPos2f(0, win_height - 12);
	draw_string(buf, ~0);

	sprintf(buf, "DETAIL=%.2g SILHOUETTE=%.2g",
		detail_threshold * win_width * win_height,
		silhouette_threshold * win_width * win_height);
	glRasterPos2f(0, 0);
	draw_string(buf, ~0);
    }

    if (help) {
	glRasterPos2f(20, win_height - 12*2);
	draw_string("h/H - TOGGLE THIS HELP MENU", ~0);
	glRasterPos2f(20, win_height - 12*3);
	draw_string("i/I - CHANGE MESH WINDING ORDER", ~0);
	glRasterPos2f(20, win_height - 12*4);
	draw_string("o/O - SHOW OCTREE STRUCTURE", ~0);
	glRasterPos2f(20, win_height - 12*5);
	draw_string("w/W - TOGGLE WIREFRAME OR FILL MODE", ~0);
	glRasterPos2f(20, win_height - 12*6);
	draw_string("c/C - TOGGLE OpenGL BACK-FACE CULLING", ~0);
	glRasterPos2f(20, win_height - 12*7);
	draw_string("f/F - TOGGLE RENDERING ORIGINAL OR SIMPLIFIED MESH", ~0);
	glRasterPos2f(20, win_height - 12*8);
	draw_string("t/T TO TOGGLE TOP VIEW", ~0);
	glRasterPos2f(20, win_height - 12*9);
	draw_string("-/+ TO DECREASE/INCREASE DETAIL THRESHOLD", ~0);
	glRasterPos2f(20, win_height - 12*10);
	draw_string("[/] TO DECREASE/INCREASE SILHOUETTE THRESHOLD", ~0);
	glRasterPos2f(20, win_height - 12*11);
	draw_string("CLICK AND DRAG 1ST MOUSE BUTTON TO CHANGE VIEW", ~0);
	glRasterPos2f(20, win_height - 12*12);
	draw_string("CLICK AND DRAG 3RD MOUSE BUTTON TO CHANGE ZOOM", ~0);
    }

    glDisable(GL_COLOR_LOGIC_OP);

    glutSwapBuffers();
}

void
render_aabb(const vec3 min, const vec3 max)
{
    glBegin(GL_QUADS);
	/* bottom face */
	glVertex3d(min[0], min[1], min[2]);
	glVertex3d(max[0], min[1], min[2]);
	glVertex3d(max[0], min[1], max[2]);
	glVertex3d(min[0], min[1], max[2]);

	/* top face */
	glVertex3d(min[0], max[1], min[2]);
	glVertex3d(min[0], max[1], max[2]);
	glVertex3d(max[0], max[1], max[2]);
	glVertex3d(max[0], max[1], min[2]);

	/* back face */
	glVertex3d(min[0], min[1], max[2]);
	glVertex3d(max[0], min[1], max[2]);
	glVertex3d(max[0], max[1], max[2]);
	glVertex3d(min[0], max[1], max[2]);

	/* front face */
	glVertex3d(min[0], min[1], min[2]);
	glVertex3d(min[0], max[1], min[2]);
	glVertex3d(max[0], max[1], min[2]);
	glVertex3d(max[0], min[1], min[2]);

	/* left face */
	glVertex3d(min[0], min[1], min[2]);
	glVertex3d(min[0], min[1], max[2]);
	glVertex3d(min[0], max[1], max[2]);
	glVertex3d(min[0], max[1], min[2]);

	/* right face */
	glVertex3d(max[0], min[1], min[2]);
	glVertex3d(max[0], max[1], min[2]);
	glVertex3d(max[0], max[1], max[2]);
	glVertex3d(max[0], min[1], max[2]);
    glEnd();
}

void
lod_render(int update,
	   const view_params *vp, int *collapsed, int *culled, int *rendered)
{
    int j, k, ti;
    octree_node *c,*n0,*n1,*n2;
    double t;
    int pupdates = 0;
    int nt;

    if (tri_index == NULL)
	tri_index = malloc(sizeof(*tri_index) * m->nt * 3);

    *collapsed = *culled = *rendered = 0;

    if (update) {
	t = get_timer();
	update_active_list(vp);
	t = get_timer()-t;
//	printf("recomputed boundary [%gs]\n", t);
    }

    /* We could be even lazier, and initialize this all to null, check for that
     * when we are lazily updating proxies, but it think it's alright since it
     * is only a one-time thing. */
    if (proxies==NULL) {
	proxies=malloc(sizeof(*proxies)*m->nv);
	for (j=0; j<m->nv; j++) {
	    c=tree->vertex_nodes[j];
	    while (c->status != STATUS_BOUNDARY)
		c=c->parent;
	    proxies[j]=c;
	}
    }

    t = get_timer();
    nt = 0;
    for (j=0; j<m->nt; j++) {
	if (tree->activators[j]->status == STATUS_INACTIVE) {
	    ++*collapsed;
	    continue;
	}
	n0=proxies[ti = m->tris[j][0]];
	if (n0->status != STATUS_BOUNDARY) {
	    if (n0->status == STATUS_ACTIVE) {
		do {
		    k = 0;
		    if (m->verts[ti][0] >= n0->bb_midpt[0]) k|=4;
		    if (m->verts[ti][1] >= n0->bb_midpt[1]) k|=2;
		    if (m->verts[ti][2] >= n0->bb_midpt[2]) k|=1;
		    n0 = n0->subtree[k];
		} while (n0->status != STATUS_BOUNDARY);
	    } else {
		assert(n0->status == STATUS_INACTIVE);
		do {
		    n0 = n0->parent;
		} while (n0->status != STATUS_BOUNDARY);
	    }
	    proxies[ti] = n0;
	    pupdates++;
	}
	n1=proxies[ti = m->tris[j][1]];
	if (n1->status != STATUS_BOUNDARY) {
	    if (n1->status == STATUS_ACTIVE) {
		do {
		    k = 0;
		    if (m->verts[ti][0] >= n1->bb_midpt[0]) k|=4;
		    if (m->verts[ti][1] >= n1->bb_midpt[1]) k|=2;
		    if (m->verts[ti][2] >= n1->bb_midpt[2]) k|=1;
		    n1 = n1->subtree[k];
		} while (n1->status != STATUS_BOUNDARY);
	    } else {
		assert(n1->status == STATUS_INACTIVE);
		do {
		    n1 = n1->parent;
		} while (n1->status != STATUS_BOUNDARY);
	    }
	    proxies[ti] = n1;
	    pupdates++;
	}
	if (n0 == n1 || n0->rep_vindex == n1->rep_vindex) {
	    ++*collapsed;
	    continue;
	}
	n2=proxies[ti = m->tris[j][2]];
	if (n2->status != STATUS_BOUNDARY) {
	    if (n2->status == STATUS_ACTIVE) {
		do {
		    k = 0;
		    if (m->verts[ti][0] >= n2->bb_midpt[0]) k|=4;
		    if (m->verts[ti][1] >= n2->bb_midpt[1]) k|=2;
		    if (m->verts[ti][2] >= n2->bb_midpt[2]) k|=1;
		    n2 = n2->subtree[k];
		} while (n2->status != STATUS_BOUNDARY);
	    } else {
		assert(n2->status == STATUS_INACTIVE);
		do {
		    n2 = n2->parent;
		} while (n2->status != STATUS_BOUNDARY);
	    }
	    proxies[ti] = n2;
	    pupdates++;
	}
	if (n0==n2 || n1==n2 || n0->rep_vindex == n2->rep_vindex ||
	    n1->rep_vindex == n2->rep_vindex) { ++*collapsed; continue; }

#if 0
	if (!vf_point_inside(vp, m->verts[n0->rep_vindex]) &&
	    !vf_point_inside(vp, m->verts[n1->rep_vindex]) &&
	    !vf_point_inside(vp, m->verts[n2->rep_vindex])) {
	    ++*culled; continue;
	}
#endif

#if 0
	glNormal3fv(n0->rep_vnormal);
	glArrayElement(n0->rep_vindex);
	glNormal3fv(n1->rep_vnormal);
	glArrayElement(n1->rep_vindex);
	glNormal3fv(n2->rep_vnormal);
	glArrayElement(n2->rep_vindex);
#else
	tri_index[nt++] = n0->rep_vindex;
	tri_index[nt++] = n1->rep_vindex;
	tri_index[nt++] = n2->rep_vindex;
#endif

	++*rendered;
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id[0]);
    glVertexPointer(3, GL_FLOAT, 0, 0);

    glEnableClientState(GL_NORMAL_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id[1]);
    glNormalPointer(GL_FLOAT, 0, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glPushMatrix();
    glTranslatef(-tree->root->bb_midpt[0],
		 -tree->root->bb_midpt[1],
		 -tree->root->bb_midpt[2]);
    glDrawElements(GL_TRIANGLES, nt, GL_UNSIGNED_INT, tri_index);
    glPopMatrix();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    t = get_timer()-t;
    printf("lod render [%gs] (%d proxy updates)\n", t, pupdates);
}

void
fullres_render()
{
    double t = get_timer();

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_id[0]);
    glVertexPointer(3, GL_FLOAT, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_id[1]);
    glNormalPointer(GL_FLOAT, 0, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_id[2]);

    glPushMatrix();
    glTranslatef(-tree->root->bb_midpt[0],
		 -tree->root->bb_midpt[1],
		 -tree->root->bb_midpt[2]);
    glDrawElements(GL_TRIANGLES, m->nt*3, GL_UNSIGNED_INT, 0);
    glPopMatrix();

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);

    t = get_timer()-t;
    printf("full-res render [%gs]\n", t);
}

void
render_octree(const octree_node *o)
{
    if (o == NULL)
	return;

    if (o->status == STATUS_BOUNDARY) {
	if (o->leaf) {
	    glColor3f(0.7f, 0.0f, 1.0f);
	    glBegin(GL_POINTS);
	    glVertex3fv(m->verts[o->rep_vindex]);
	    glEnd();
	} else {
	    vec3 min, max;
	    aabb_midpt_to_corners(min, max, o->bb_midpt, o->bb_extent);
	    glColor3f(0.0f, 0.0f, 1.0f);
	    render_aabb(min, max);
	}
    } else {
	int j;

	for (j=0; j<8; j++)
	    render_octree(o->subtree[j]);
    }
}

void
reshape(int w, int h)
{
    win_width = w;
    win_height = h;
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

void
key_press(unsigned char key, int x, int y)
{
    (void)x;
    (void)y;

    switch (key) {
	case 27:
	case 'q':
	case 'Q':
	    exit(0);

	case 'h':
	case 'H':
	    help = !help;
	    break;

	case 'l':
	case 'L':
	    lock = !lock;
	    break;

	case 't':
	case 'T':
	    top_view = !top_view;
	    break;

	case 'f':
	case 'F':
	    fullres = !fullres;
	    break;

	case 'o':
	case 'O':
	    draw_octree = !draw_octree;
	    break;

	case 'w':
	case 'W':
	    wire = (wire+1) % 3;
	    break;

	case 'c':
	case 'C':
	    bf_cull = !bf_cull;
	    break;

	case 'i':
	case 'I':
	    octree_free(tree);
	    free(proxies);
	    proxies=NULL;

	    mesh_flip(m);
	    tree=octree_create(m);
	    while (active_list) {
		active_node *next = active_list->next;
		free(active_list);
		active_list = next;
	    }
	    delete_vbos();
	    create_vbos();
	    break;

	case 'p':
	case 'P':
	    pixel_shade = !pixel_shade;
	    break;

	case '-':
	    detail_threshold /= 0.9;
	    break;

	case '+':
	    detail_threshold *= 0.9;
	    break;

	case '[':
	    silhouette_threshold /= 0.9;
	    break;

	case ']':
	    silhouette_threshold *= 0.9;
	    break;

	default:
	    return;
    }

    glutPostRedisplay();
}

int
tree_depth(const octree_node *o)
{
    int max = 0, d, j;

    if (o == NULL) return 0;
    if (o->leaf) return 1;

    for (j=0; j<8; j++) {
	d = tree_depth(o->subtree[j]);
	if (max < d) max = d;
    }
    return 1+max;
}
