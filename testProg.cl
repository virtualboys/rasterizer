
// column major
float4 vec4_mul_mat4(const float4 vec, global const float4* mat) {
	return (float4)(vec.x * mat[0][0] + vec.y * mat[1][0] + vec.z * mat[2][0] + vec.w * mat[3][0],
					vec.x * mat[0][1] + vec.y * mat[1][1] + vec.z * mat[2][1] + vec.w * mat[3][1],
					vec.x * mat[0][2] + vec.y * mat[1][2] + vec.z * mat[2][2] + vec.w * mat[3][2],
					vec.x * mat[0][3] + vec.y * mat[1][3] + vec.z * mat[2][3] + vec.w * mat[3][3]);
}

float3 barycentric(float2 A, float2 B, float2 C, float2 P) {
    float3 s[2];
    for (int i=2; i--; ) {
        s[i][0] = C[i]-A[i];
        s[i][1] = B[i]-A[i];
        s[i][2] = A[i]-P[i];
    }
    float3 u = cross(s[0], s[1]);
    if (fabs(u[2])>1e-2) // 
        //dont forget that u[2] is integer. If it is zero then triangle ABC is degenerate
        return (float3)(1.f-(u.x+u.y)/u.z, u.y/u.z, u.x/u.z);
    return (float3)(-1,1,1); // in this case generate negative coordinates, it will be thrown away by the rasterizator
}

float3 interpolateBary(float3 a, float3 b, float3 c, float3 bar) {
	return (float3)(a.x * bar.x + b.x * bar.y + c.x * bar.z, 
					a.y * bar.x + b.y * bar.y + c.y * bar.z, 
					a.z * bar.x + b.z * bar.y + c.z * bar.z);
}

void printVec(float4 vec) {
	printf("vec: %f %f %f %f\n", vec.x, vec.y, vec.z, vec.w);
}

kernel void rasterizer(
		ulong nTris,
		int width,
		int height,
		float offset,
		global float* zbuffer,
		global const float4* viewport, 
		global const float* vertices,
		global const float* normalsIn,
		global const int* indices,
		global char* screen,
		global float* normalsOut)
{
	size_t faceInd = get_global_id(0);

	float3 homoZs;
	float4 vsVP[3];
	float3 vs[3];
	float3 ns[3];
	for(int i = 0; i < 3; i++) {
		// indices are vertex/uv/normal
		int vertInd = indices[faceInd*9+i*3];
		int uvInd = indices[faceInd*9+i*3+1];
		int normalInd = indices[faceInd*9+i*3+2];
		normalInd = vertInd;
		
		float4 vertHomo = (float4)(vertices[vertInd*4], vertices[vertInd*4+1], vertices[vertInd*4+2], vertices[vertInd*4+3]);
		
		homoZs[i] = vertHomo.z;
		vertHomo = vec4_mul_mat4(vertHomo, viewport);
		vsVP[i] = vertHomo;
		vs[i] = vertHomo.xyz / vertHomo.w;

		ns[i] = (float3)(normalsIn[normalInd*3], normalsIn[normalInd*3+1], normalsIn[normalInd*3+2]);
	}

	float2 bboxmin = (float2)(INFINITY,INFINITY);
	float2 bboxmax = (float2)(-INFINITY,-INFINITY);

	float2 clampCoords = (float2)(width-1, height-1);

	for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            bboxmin[j] = max(0.f, min(bboxmin[j], vs[i][j]));
            bboxmax[j] = min(clampCoords[j], max(bboxmax[j], vs[i][j]));
        }
    }

    int2 pix;
    for (pix.x=bboxmin.x; pix.x<=bboxmax.x; pix.x++) {
        for (pix.y=bboxmin.y; pix.y<=bboxmax.y; pix.y++) {
            float3 bc_screen  = barycentric(vs[0].xy, vs[1].xy, vs[2].xy, (float2)(pix.x,pix.y));
            float3 bc_clip    = (float3)(bc_screen.x/vsVP[0][3], bc_screen.y/vsVP[1][3], bc_screen.z/vsVP[2][3]);
            //printf("bccl: %f %f %f\n", bc_clip.x, bc_clip.y, bc_clip.z);

            bc_clip = bc_clip/(bc_clip.x+bc_clip.y+bc_clip.z);

    		
            float frag_depth = dot(homoZs, bc_clip);// clipc[2]*bc_clip;
            //printf("frag: %f\n", frag_depth);
            int pixInd = pix.x+pix.y*width;

            if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0 || zbuffer[pixInd]>frag_depth) continue;

            zbuffer[pixInd] = frag_depth;
            zbuffer[(int)(pixInd * (1.0f + offset))] = frag_depth;
            screen[pixInd*3] = (.5f + .5f * bc_screen.x) * 255;
            screen[pixInd*3+1] = (.5f + .5f * bc_screen.y) * 255;
            screen[pixInd*3+2] = (.5f + .5f * bc_screen.z) * 255;

            float3 n = interpolateBary(ns[0], ns[1], ns[2], bc_clip);
            normalsOut[pixInd*3] = n.x;
            normalsOut[pixInd*3+1] = n.y;
            normalsOut[pixInd*3+2] = n.z;
        }
    }
}

kernel void fragment(
		int width,
		int height,
		float3 viewDir,
		global const float* zBuffer,
		global const float* normals,
		global char* color
	)
{
	size_t i = get_global_id(0);
	int x = i % width;
	int y = i / width;

	float3 n = (float3)(normals[i*3], normals[i*3+1], normals[i*3+2]);

	// printf("zbuf: %f", zBuffer[i]);
	if(zBuffer[i] > .1f) {
		float val = dot(n, viewDir);
		// color[i*3] = (char)(n.x * 255);
		// color[i*3+1] = (char)(n.y * 255);
		// color[i*3+2] = (char)(n.z * 255);	
		color[i*3] = val * (char)(255);
		color[i*3+1] = val * (char)(255);
		color[i*3+2] = val * (char)(255);	
	}
}

kernel void vertex(
		ulong n,
		global const float4* mvp, 
		global const float4* modelMat,
		global const float* vertices,
		global const float* normalsIn,
		global float* positions,
		global float* normalsOut
	)
{
	size_t i = get_global_id(0);
	float4 vert = (float4)(vertices[i*3], vertices[i*3+1], vertices[i*3+2], 1.0f);
	vert = vec4_mul_mat4(vert, mvp);

	positions[i*4] = vert.x;
	positions[i*4+1] = vert.y;
	positions[i*4+2] = vert.z;
	positions[i*4+3] = vert.w;

	float4 normal = (float4)(normalsIn[i*3], normalsIn[i*3+1], normalsIn[i*3+2],1.0f);
	normal = vec4_mul_mat4(normal, modelMat);
	normalize(normal);

	normalsOut[i*3] = normal.x;
	normalsOut[i*3+1] = normal.y;
	normalsOut[i*3+2] = normal.z;
}
