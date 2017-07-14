#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mesh.h"

static void face_normals(mesh *m);
static void vertex_normals(mesh *m);
static void vertex_bbox(mesh *m);

static const char *const kFieldOrder[] = {
  "x","y","z", "nx","ny","nz", "red","green","blue", "tu","tv", NULL
};

mesh *
mesh_load(const char *file)
{
    vec3 *verts=NULL;
    vec3 *vnormals=NULL;
    color3u *vcolors=NULL;
    texcoord2f *vtexcoords=NULL;
    index3u *tris=NULL;
    FILE *fp=NULL;
    char buf[256]={0}, type[256], field[256], *ptr;
    mesh *m=NULL;
    if ((fp = fopen(file, "r")) == NULL)
	goto fail;

    if (!fgets(buf, sizeof(buf), fp) || strcmp(buf, "ply\n") ||
	!fgets(buf, sizeof(buf), fp) || strcmp(buf, "format ascii 1.0\n"))
	goto fail;

    do {
	fgets(buf, sizeof(buf), fp);
    } while (strncmp(buf, "comment", 7) == 0);

    /* read number of vertices */
    const int nv=(strncmp(buf, "element vertex", 14) == 0) ? strtol(buf+15, NULL, 10) : -1;

    if (nv <= 0)
	goto fail;

    short order[11];
    for (int i=0; i<11; i++)
	order[i]=-1;

    /* read order of properties */
    for (int i=0; fgets(buf,sizeof(buf),fp) && strncmp(buf,"property ",9)==0; i++) {
	sscanf(buf, "property %s %s\n", type, field);
	for (int j=0; kFieldOrder[j]; j++)
	    if (strcmp(field, kFieldOrder[j]) == 0) {
		if (order[j] != -1)
		    goto fail;
		order[j] = i;
		break;
	    }
    }

    const int nt=(strncmp(buf, "element face ", 13) == 0) ? strtol(buf+13, NULL, 10) : -1;
    if (nt <= 0)
	goto fail;

    fgets(buf, sizeof(buf), fp);
    if (strncmp(buf, "property list", 13) != 0)
	goto fail;

    do {
	fgets(buf, sizeof(buf), fp);
    } while (strncmp(buf, "end_header", 10) != 0);

#if 0
    printf("order={%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d}\n",
	   order[0], order[1], order[2], order[3], order[4], order[5],
	   order[6], order[7], order[8], order[9], order[10]);
#endif

    /* allocate things we will need */
    /* not enough fields provided (must have x,y,z coordinates) */
    if (order[0]<0 || order[1]<0 || order[2]<0)
	goto fail;
    verts=malloc(sizeof(*verts)*nv);

    int nf=3;

    if (order[3]>=0 && order[4]>=0 && order[5]>=0) {
	vnormals=malloc(sizeof(*vnormals)*nv);
	nf+=3;
    }

    if (order[6]>=0 && order[7]>=0 && order[8]>=0) {
	vcolors=malloc(sizeof(*vcolors)*nv);
	nf+=3;
    }

    if (order[9]>=0 && order[10]>=0) {
	vtexcoords=malloc(sizeof(*vtexcoords)*nv);
	nf+=2;
    }

    /* read vertices */
    for (int i=0; i<nv; i++) {
        double fields[11];
	if (!fgets(buf, sizeof(buf), fp))
	    goto fail;
	ptr=buf;
	for (int j=0; j<nf; j++)
	    fields[j]=strtod(ptr,&ptr);

	verts[i][0]=fields[order[0]];
	verts[i][1]=fields[order[1]];
	verts[i][2]=fields[order[2]];

	if (vnormals) {
	    vnormals[i][0]=fields[order[3]];
	    vnormals[i][1]=fields[order[4]];
	    vnormals[i][2]=fields[order[5]];
	}
	if (vcolors) {
	    vcolors[i][0]=(unsigned char)fields[order[6]];
	    vcolors[i][1]=(unsigned char)fields[order[7]];
	    vcolors[i][2]=(unsigned char)fields[order[8]];
	}
	if (vtexcoords) {
	    vtexcoords[i][0]=fields[order[9]];
	    vtexcoords[i][1]=fields[order[10]];
	}
    }

    tris=malloc(sizeof(*tris)*nt);
    /* read triangles */
    for (int i=0; i<nt; i++) {
	if (!fgets(buf, sizeof(buf), fp))
	    goto fail;
        int k;
	int r = sscanf(buf, "%u %u %u %u", &k, &tris[i][0], &tris[i][1], &tris[i][2]);
	if (k != 3 || r != 4 ||
	    tris[i][0]==tris[i][1] ||
	    tris[i][0]==tris[i][2] ||
	    tris[i][1]==tris[i][2])
	    goto fail;
    }
    fclose(fp);

#if 0
    /* check to see if every vertex is used. */
    for (i=0; i<nv; i++) {
	for (j=0; j<nt; j++)
	    if (tris[j][0]==i || tris[j][1]==i || tris[j][2]==i)
		break;
	if (j==nt)
	    printf("mesh ``%s'' warning: unused vertex #%d\n", file, i);
    }
#endif

    m = malloc(sizeof(*m));
    m->nv = nv;
    m->verts = verts;
    m->vnormals = vnormals;
    m->vcolors = vcolors;
    m->vtexcoords = vtexcoords;
    m->nt = nt;
    m->tris = tris;
    m->tnormals = malloc(sizeof(*m->tnormals)*nt);

    vertex_bbox(m);
    face_normals(m);
    if (!m->vnormals) {
	m->vnormals = malloc(sizeof(*m->vnormals)*nv);
	vertex_normals(m);
    }
    /* normalize face normals */
    for (int i=0; i<nt; i++)
	VecNormalize(m->tnormals[i]);

    return m;

fail:
    if (fp != NULL)
	fclose(fp);
    if (verts != NULL)
	free(verts);
    if (tris != NULL)
	free(tris);
    if (m != NULL)
	free(m);
    return NULL;
}

static void
face_normals(mesh *m)
{
    for (int j=0; j<m->nt; j++) {
	real* v0=m->verts[m->tris[j][0]];
	real* v1=m->verts[m->tris[j][1]];
	real* v2=m->verts[m->tris[j][2]];
	Normal(m->tnormals[j], v0, v1, v2);
//	VecNormalize(m->tnormals[j]);
    }
}

static void
vertex_normals(mesh *m)
{
    for (int j=0; j<m->nv; j++)
	m->vnormals[j][0]=m->vnormals[j][1]=m->vnormals[j][2]=0;

    for (int j=0; j<m->nt; j++) {
	int k = m->tris[j][0];
	VecAdd(m->vnormals[k], m->vnormals[k], m->tnormals[j]);
	k = m->tris[j][1];
	VecAdd(m->vnormals[k], m->vnormals[k], m->tnormals[j]);
	k = m->tris[j][2];
	VecAdd(m->vnormals[k], m->vnormals[k], m->tnormals[j]);
    }

    for (int j=0; j<m->nv; j++)
	VecNormalize(m->vnormals[j]);
}

static void
vertex_bbox(mesh *m)
{
    VecSet(m->min, m->verts[0]);
    VecSet(m->max, m->verts[0]);

    for (int j=0; j<m->nv; j++) {
	if (m->min[0]>m->verts[j][0]) m->min[0]=m->verts[j][0]; else
	if (m->max[0]<m->verts[j][0]) m->max[0]=m->verts[j][0];

	if (m->min[1]>m->verts[j][1]) m->min[1]=m->verts[j][1]; else
	if (m->max[1]<m->verts[j][1]) m->max[1]=m->verts[j][1];

	if (m->min[2]>m->verts[j][2]) m->min[2]=m->verts[j][2]; else
	if (m->max[2]<m->verts[j][2]) m->max[2]=m->verts[j][2];
    }
}

void
mesh_free(mesh *m)
{
    if (m) {
	if (m->verts)
	    free(m->verts);
	if (m->vnormals)
	    free(m->vnormals);
	if (m->vcolors)
	    free(m->vcolors);
	if (m->vtexcoords)
	    free(m->vtexcoords);
	if (m->tris)
	    free(m->tris);
	if (m->tnormals)
	    free(m->tnormals);
	free(m);
    }
}

void
mesh_flip(mesh *m)
{
    /* flip all normals and change triangle orientation */
    for (int j=0; j<m->nt; j++) {
	int k=m->tris[j][1];
	m->tris[j][1]=m->tris[j][2];
	m->tris[j][2]=k;
	VecScale(m->tnormals[j], m->tnormals[j], -1);
    }
    for (int j=0; j<m->nv; j++) {
	VecScale(m->vnormals[j], m->vnormals[j], -1);
    }
}
