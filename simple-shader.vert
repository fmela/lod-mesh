varying vec3 V;
varying vec3 N;

void main(void)
{
	V = (gl_ModelViewMatrix * gl_Vertex).xyz;
 	N = normalize(gl_NormalMatrix * gl_Normal);
	gl_Position = ftransform();
}
