#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mesh.h"

static void face_normals(mesh *m);
static void vertex_normals(mesh *m);
static void vertex_bbox(mesh *m);

enum Field {
    X,Y,Z,
    NX,NY,NZ,
    RED,GREEN,BLUE,
    IGNORED
};

static const char *const kFieldStrings[] = {
  "x","y","z",
  "nx","ny","nz",
  "red","green","blue",
  "ignored"
};
// static const unsigned kNumFieldStrings = sizeof(kFieldStrings)/sizeof(kFieldStrings[0]);

enum FieldType {
    BYTE,
    FLOAT,
};
static const char *const kFieldTypeStrings[] = {
  "Byte", "Float",
};
// static const unsigned kNumFieldTypeStrings = sizeof(kFieldTypeStrings)/sizeof(kFieldTypeStrings[0]);

struct FieldAndType {
    enum Field field;
    enum FieldType type;
};

static const char kComment[] = "comment";
static const char kElementVertex[] = "element vertex ";
static const char kElementFace[] = "element face ";
static const char kPropertyList[] = "property list ";

mesh *
mesh_load(const char *file)
{
    mesh *const m=malloc(sizeof(*m));
    FILE *const fp=fopen(file, "r");
    if (!m || !fp)
	goto fail;
    memset(m, 0, sizeof(*m));

    char buf[256]={0};
    if (!fgets(buf, sizeof(buf), fp) || strcmp(buf, "ply\n"))
	goto fail;
    if (!fgets(buf, sizeof(buf), fp))
	goto fail;
    bool ascii = !strcmp(buf, "format ascii 1.0\n");
    bool binary_le = !strcmp(buf, "format binary_little_endian 1.0\n");
    bool binary_be = !strcmp(buf, "format binary_big_endian 1.0\n");
    if (!ascii && !binary_le && !binary_be) {
	fprintf(stderr, "expected ascii, binary_little_endian, or binary_big_endian 1.0 format!\n");
	goto fail;
    }
    do {
	fgets(buf, sizeof(buf), fp);
    } while (strncmp(buf, kComment, strlen(kComment)) == 0);

    /* read number of vertices */
    if (strncmp(buf, kElementVertex, strlen(kElementVertex)) == 0) {
	m->nv=strtoul(buf+strlen(kElementVertex), NULL, 10);
    } else {
	fprintf(stderr, "Expected line starting with: '%s', got: %s\n", kElementVertex, buf);
    }
    if (m->nv <= 0)
	goto fail;

    const unsigned kMaxVertexFields = 32;
    struct FieldAndType fields[kMaxVertexFields];
    memset(fields, 0, sizeof(fields));

    int xField = -1, yField = -1, zField = -1;
    int nxField = -1, nyField = -1, nzField = -1;
    int rField = -1, gField = -1, bField = -1;
    unsigned nfields = 0;
    for (unsigned i=0; fgets(buf,sizeof(buf),fp) && strncmp(buf,"property ",9)==0; ++i) {
	if (nfields == kMaxVertexFields) {
	    fprintf(stderr, "Too many per-vertex fields\n");
	    goto fail;
	}
	char type[256]={0}, field[256]={0};
	sscanf(buf, "property %255s %255s\n", type, field);
	bool isFloat = false;
	bool isByte = false;
	if ((strcmp("float", type) == 0 || strcmp("float32", type) == 0)) {
	    isFloat = true;
	} else if (strcmp("uint8", type) == 0 || strcmp("uchar", type) == 0) {
	    isByte = true;
	} else {
	    fprintf(stderr, "only float and uchar properties are supported at this time\n");
	    goto fail;
	}
	if (!strcmp("x", field)) {
	    if (xField != -1 || !isFloat) goto fail;
	    fields[nfields].field = X; fields[nfields].type = FLOAT; xField = nfields++;
	} else if (!strcmp("y", field)) {
	    if (yField != -1 || !isFloat) goto fail;
	    fields[nfields].field = Y; fields[nfields].type = FLOAT; yField = nfields++;
	} else if (!strcmp("z", field)) {
	    if (zField != -1 || !isFloat) goto fail;
	    fields[nfields].field = Z; fields[nfields].type = FLOAT; zField = nfields++;
	} else if (!strcmp("nx", field)) {
	    if (nxField != -1 || !isFloat) goto fail;
	    fields[nfields].field = NX; fields[nfields].type = FLOAT; nxField = nfields++;
	} else if (!strcmp("ny", field)) {
	    if (nyField != -1 || !isFloat) goto fail;
	    fields[nfields].field = NY; fields[nfields].type = FLOAT; nyField = nfields++;
	} else if (!strcmp("nz", field)) {
	    if (nzField != -1 || !isFloat) goto fail;
	    fields[nfields].field = NZ; fields[nfields].type = FLOAT; nzField = nfields++;
	} else if (!strcmp("red", field)) {
	    if (rField != -1 || !isByte) goto fail;
	    fields[nfields].field = RED; fields[nfields].type = BYTE; rField = nfields++;
	} else if (!strcmp("green", field)) {
	    if (gField != -1 || !isByte) goto fail;
	    fields[nfields].field = GREEN; fields[nfields].type = BYTE; gField = nfields++;
	} else if (!strcmp("blue", field)) {
	    if (bField != -1 || !isByte) goto fail;
	    fields[nfields].field = BLUE; fields[nfields].type = BYTE; bField = nfields++;
	} else {
	    fprintf(stderr, "Warning: ignoring field %s %s\n", type, field);
	    fields[nfields].field = IGNORED; fields[nfields].type = isFloat ? FLOAT : BYTE; nfields++;
	}
    }
    printf("{x,y,z}={%d,%d,%d} {nx,ny,nz}={%d,%d,%d} {r,g,b}={%d,%d,%d}\n",
	   xField, yField, zField,
	   nxField, nyField, nzField,
	   rField, gField, bField);
    for (unsigned i=0; i<nfields; ++i) {
	printf("Field %u: %s %s\n",
	       i, kFieldStrings[fields[i].field], kFieldTypeStrings[fields[i].type]);
    }
    if (xField<0 || yField<0 || zField<0) {
	fprintf(stderr, "Missing X, Y, or Z field(s)\n");
	goto fail;
    }
    m->verts=malloc(sizeof(*m->verts)*m->nv);

    if (nxField>=0 || nyField>=0 || nzField>=0) {
	if (nxField<0 || nyField<0 || nzField<0) {
	    fprintf(stderr, "Bad NX, NY, or NZ field(s)\n");
	    goto fail;
	}
	m->vnormals=malloc(sizeof(*m->vnormals)*m->nv);
    }

    if (rField>=0 || gField>=0 || bField>=0) {
	if (rField<0 || gField<0 || bField<0) {
	    fprintf(stderr, "Bad R, G, or B field(s)\n");
	    goto fail;
	}
	m->vcolors=malloc(sizeof(*m->vcolors)*m->nv);
    }

    if (strncmp(buf, kElementFace, strlen(kElementFace)) == 0) {
	m->nt=strtoul(buf+strlen(kElementFace), NULL, 10);
    } else {
	fprintf(stderr, "Expected: '%s', but got: %s\n", kElementFace, buf);
    }
    if (m->nt <= 0)
	goto fail;

    bool got_face_format = false;
    unsigned before_tri_skip = 0, after_tri_skip = 0;
    for (;;) {
	if (!fgets(buf, sizeof(buf), fp))
	    goto fail;
	if (strncmp(buf, kPropertyList, strlen(kPropertyList)) == 0) {
	    char s0[32] = {0}, s1[32] = {0}, s2[32] = {0};
	    if (sscanf(buf + strlen(kPropertyList), "%31s %31s %31s\n", s0, s1, s2) != 3) {
		fprintf(stderr, "Ignoring property list line: %s", buf);
		continue;
	    }
	    if (strcmp(s0, "uchar") && strcmp(s0, "uint8")) {
		fprintf(stderr, "only uchar or uint8 vertex count type is supported\n");
		goto fail;
	    }
	    if (strcmp(s1, "int") && strcmp(s1, "int32")) {
		fprintf(stderr, "only int or int32 vertex index type is supported\n");
		goto fail;
	    }
	    if (strcmp(s2, "vertex_index") && strcmp(s2, "vertex_indices")) {
		fprintf(stderr, "only vertex_index or vertex_indices are supported\n");
		goto fail;
	    }
	    got_face_format = true;
	} else if (strncmp(buf, "property uchar ", 15) == 0 ||
		   strncmp(buf, "property uint8 ", 15) == 0) {
	    fprintf(stderr, "ignoring: %s\n", buf);
	    if (got_face_format)
		after_tri_skip += 1;
	    else
		before_tri_skip += 1;
	} else if (strncmp(buf, "property float ", 15) == 0 ||
		   strncmp(buf, "property float32 ", 17) == 0) {
	    fprintf(stderr, "ignoring: %s\n", buf);
	    if (got_face_format)
		after_tri_skip += 4;
	    else
		before_tri_skip += 4;
	} else if (strcmp(buf, "end_header\n") == 0) {
	    break;
	} else {
	    fprintf(stderr, "Warning: ignoring PLY line: %s\n", buf);
	}
    }
    if (!got_face_format)
	goto fail;

    while (strcmp(buf, "end_header\n")) {
	fprintf(stderr, "Warning: ignoring PLY line: %s\n", buf);
	if (!fgets(buf, sizeof(buf), fp))
	    goto fail;
    }

    /* read vertices */
    if (ascii) {
	for (unsigned i=0; i<m->nv; i++) {
	    double dfields[11];
	    if (!fgets(buf, sizeof(buf), fp))
		goto fail;
	    char *ptr=buf;
	    for (unsigned j=0; j<nfields; j++)
		dfields[j]=strtod(ptr,&ptr);

	    m->verts[i][0]=dfields[xField];
	    m->verts[i][1]=dfields[yField];
	    m->verts[i][2]=dfields[zField];

	    if (m->vnormals) {
		m->vnormals[i][0]=dfields[nxField];
		m->vnormals[i][1]=dfields[nyField];
		m->vnormals[i][2]=dfields[nzField];
	    }
	    if (m->vcolors) {
		m->vcolors[i][0]=(uint8_t)dfields[rField];
		m->vcolors[i][1]=(uint8_t)dfields[gField];
		m->vcolors[i][2]=(uint8_t)dfields[bField];
	    }
	    assert(!m->vtexcoords);
	}
    } else {
	for (unsigned i=0; i<m->nv; ++i) {
	    for (unsigned j=0; j<nfields; ++j) {
		union {
		    float f;
		    uint8_t b[4];
		} v;
		if (fields[j].type == FLOAT) {
		    if (fread(&v.f, sizeof(v.f), 1, fp) != 1) {
			fprintf(stderr, "float I/O error\n");
			goto fail;
		    }
		    if (binary_be) { // XXX assumes little-endian machine.
			uint8_t t;
			t = v.b[0]; v.b[0] = v.b[3]; v.b[3] = t;
			t = v.b[1]; v.b[1] = v.b[2]; v.b[2] = t;
		    }
		} else {
		    assert(fields[j].type == BYTE);
		    if (fread(&v.b[0], 1, 1, fp) != 1) {
			fprintf(stderr, "byte I/O error\n");
			goto fail;
		    }
		}
		switch (fields[j].field) {
		    case X: m->verts[i][0] = v.f; break;
		    case Y: m->verts[i][1] = v.f; break;
		    case Z: m->verts[i][2] = v.f; break;

		    case NX: m->vnormals[i][0] = v.f; break;
		    case NY: m->vnormals[i][1] = v.f; break;
		    case NZ: m->vnormals[i][2] = v.f; break;

		    case RED: m->vcolors[i][0] = v.b[0]; break;
		    case GREEN: m->vcolors[i][1] = v.b[0]; break;
		    case BLUE: m->vcolors[i][2] = v.b[0]; break;

		    case IGNORED: break;

		    default:
			assert(false && "This should never happen!");
		}
	    }
	}
    }

    m->tris=malloc(sizeof(*m->tris)*m->nt);
    /* read triangles */
    for (unsigned i=0; i<m->nt; i++) {
	if (ascii) {
	    if (!fgets(buf, sizeof(buf), fp))
		goto fail;
	    unsigned num_vertices;
	    int r = sscanf(buf, "%u %u %u %u", &num_vertices,
			   &m->tris[i][0], &m->tris[i][1], &m->tris[i][2]);
	    if (r != 4 || num_vertices != 3) {
		fprintf(stderr, "triangle %u failed to read: %s\n", i, buf);
		goto fail;
	    }
	} else {
	    if (before_tri_skip + (i > 0 ? after_tri_skip : 0))
		fseek(fp, before_tri_skip + (i > 0 ? after_tri_skip : 0), SEEK_CUR);

	    uint8_t num_vertices;
	    union { uint8_t b[4]; uint32_t i; } t[3];
	    if (fread(&num_vertices, sizeof(num_vertices), 1, fp) != 1 || num_vertices != 3) {
		fprintf(stderr, "triangle vertex I/O error: %u\n", num_vertices);
		goto fail;
	    }
	    if (fread(&t[0], sizeof(t), 1, fp) != 1) {
		fprintf(stderr, "triangle index I/O error\n");
		goto fail;
	    }
	    if (after_tri_skip && i+1 == m->nt)
		fseek(fp, after_tri_skip, SEEK_CUR);
	    // printf("tris=%u %u %u\n", t[0].i, t[1].i, t[2].i);
	    if (binary_be) {
		for (unsigned j=0; j<3; ++j) {
		    uint8_t tmp;
		    tmp = t[j].b[0]; t[j].b[0] = t[j].b[3]; t[j].b[3] = tmp;
		    tmp = t[j].b[1]; t[j].b[1] = t[j].b[2]; t[j].b[2] = tmp;
		}
		// printf("tris=%u %u %u\n", t[0].i, t[1].i, t[2].i);
	    }
	    for (unsigned j=0; j<3; ++j)
		m->tris[i][j] = t[j].i;
	}
	if (m->tris[i][0]==m->tris[i][1] ||
	    m->tris[i][0]==m->tris[i][2] ||
	    m->tris[i][1]==m->tris[i][2]) {
	    fprintf(stderr, "triangle %u is degenerate: %u %u %u\n",
		    i, m->tris[i][0], m->tris[i][1], m->tris[i][2]);
	    goto fail;
	}
	// Hack for killeroo
	// m->tris[i][0]--;
	// m->tris[i][1]--;
	// m->tris[i][2]--;
	for (unsigned j = 0; j < 3; ++j) {
	    if (m->tris[i][j] >= m->nv) {
		fprintf(stderr, "triangle %u index %u (%u) out of range\n",
			i, j, m->tris[i][j]);
		goto fail;
	    }
	}
    }

    m->tnormals = malloc(sizeof(*m->tnormals)*m->nt);
    if (!m->tnormals)
	goto fail;

    vertex_bbox(m);
    face_normals(m);
    if (!m->vnormals) {
	m->vnormals = malloc(sizeof(*m->vnormals)*m->nv);
	if (!m->vnormals)
	    goto fail;
	vertex_normals(m);
    }
    /* normalize face normals */
    for (unsigned i=0; i<m->nt; i++)
	VecNormalize(m->tnormals[i]);

    long position = ftell(fp);
    fseek(fp, 0, SEEK_END);
    if (ftell(fp) != position)
	fprintf(stderr, "warning: %ld byte(s) not read\n", ftell(fp) - position);
    fclose(fp);
    return m;

fail:
    mesh_free(m);
    fclose(fp);
    return NULL;
}

static void
face_normals(mesh *m)
{
    for (unsigned j=0; j<m->nt; j++) {
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
    for (unsigned j=0; j<m->nv; j++)
	m->vnormals[j][0]=m->vnormals[j][1]=m->vnormals[j][2]=0;

    for (unsigned j=0; j<m->nt; j++) {
	unsigned k = m->tris[j][0];
	VecAdd(m->vnormals[k], m->vnormals[k], m->tnormals[j]);
	k = m->tris[j][1];
	VecAdd(m->vnormals[k], m->vnormals[k], m->tnormals[j]);
	k = m->tris[j][2];
	VecAdd(m->vnormals[k], m->vnormals[k], m->tnormals[j]);
    }

    for (unsigned j=0; j<m->nv; j++)
	VecNormalize(m->vnormals[j]);
}

static void
vertex_bbox(mesh *m)
{
    VecSet(m->min, m->verts[0]);
    VecSet(m->max, m->verts[0]);

    for (unsigned j=0; j<m->nv; j++) {
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
	free(m->verts);
	free(m->vnormals);
	free(m->vcolors);
	free(m->vtexcoords);
	free(m->tris);
	free(m->tnormals);
	free(m);
    }
}

void
mesh_flip(mesh *m)
{
    /* flip all normals and change triangle orientation */
    for (unsigned j=0; j<m->nt; j++) {
	unsigned k=m->tris[j][1];
	m->tris[j][1]=m->tris[j][2];
	m->tris[j][2]=k;
	VecScale(m->tnormals[j], m->tnormals[j], -1);
    }
    for (unsigned j=0; j<m->nv; j++) {
	VecScale(m->vnormals[j], m->vnormals[j], -1);
    }
}
