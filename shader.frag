varying vec3 V;
varying vec3 N;

void main(void)
{
	N = normalize(N);
	vec3 E = normalize(-V);
	vec3 L = normalize(gl_LightSource[0].position.xyz - V);
	vec3 R = reflect(-L,N);

	float diffuse = max(dot(N,L), 0);
	float specular = pow(max(dot(R,E), 0.0), gl_FrontMaterial.shininess*.22);

	gl_FragColor =
		gl_FrontLightProduct[0].ambient +
		gl_FrontLightProduct[0].diffuse * diffuse +
		gl_FrontLightProduct[0].specular * specular;
}
