#ifndef _SHADER_H_
#define _SHADER_H_

#include <GL/gl.h>
#include <GL/glext.h>

typedef struct {
    GLcharARB	*vshader;
    int		 vshader_size;
    GLcharARB	*fshader;
    int		 fshader_size;
} shader;

shader *shader_load(const char *shader_name);
void	shader_free(shader *s);

#endif // !_SHADER_H_
