#ifndef _SHADER_H_
#define _SHADER_H_

#if defined(__APPLE__)
# include <OpenGL/gl.h>
#else
# include <GL/gl.h>
# include <GL/glext.h>
#endif

typedef struct {
    GLchar	*vshader;
    int		 vshader_size;
    GLchar	*fshader;
    int		 fshader_size;
} shader;

shader *shader_load(const char *shader_name);
void	shader_free(shader *s);

#endif // !_SHADER_H_
