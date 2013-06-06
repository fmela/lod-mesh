#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shader.h"

shader *
shader_load(const char *shader_name)
{
    int vsize, fsize, n;
    char buf[512];
    FILE *vfp, *ffp;
    shader *s;

    snprintf(buf, sizeof(buf), "%s.vert", shader_name);
    if ((vfp = fopen(buf, "r")) == NULL)
	return NULL;

    snprintf(buf, sizeof(buf), "%s.frag", shader_name);
    if ((ffp = fopen(buf, "r")) == NULL) {
	fclose(vfp);
	return NULL;
    }

    vsize=0;
    for (;;) {
	if ((n = fread(buf, 1, sizeof(buf), vfp)) <= 0) {
	    if (n<0) vsize=0;
	    break;
	}
	vsize += n;
    }
    if (vsize == 0) {
	fclose(vfp);
	fclose(ffp);
	return NULL;
    }

    fsize=0;
    for (;;) {
	if ((n = fread(buf, 1, sizeof(buf), ffp)) <= 0) {
	    if (n<0) fsize=0;
	    break;
	}
	fsize += n;
    }
    if (fsize == 0) {
	fclose(vfp);
	fclose(ffp);
	return NULL;
    }

    s = malloc(sizeof(*s));
    s->vshader = malloc(vsize+1);
    s->fshader = malloc(fsize+1);

    rewind(vfp);
    fread(s->vshader, 1, vsize, vfp);
    fclose(vfp);
    s->vshader[vsize] = '\0';
    s->vshader_size = vsize;

    rewind(ffp);
    fread(s->fshader, 1, fsize, ffp);
    fclose(ffp);
    s->fshader[fsize] = '\0';
    s->fshader_size = fsize;

    return s;
}

void
shader_free(shader *s)
{
    free(s->fshader);
    free(s->vshader);
    free(s);
}
