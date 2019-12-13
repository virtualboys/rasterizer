
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

void printVec(float4 vec) {
	printf("vec: %f %f %f %f\n", vec.x, vec.y, vec.z, vec.w);
}

kernel void rasterizer(
		ulong nTris,
		int width,
		int height,
		global float* zbuffer,
		global const float4* viewport, 
		global const float* vertices,
		global const int* indices,
		global char* screen)
{
	size_t faceInd = get_global_id(0);

	float3 homoZs;
	float4 vsVP[3];
	float3 vs[3];
	for(int i = 0; i < 3; i++) {
		// indices are vertex/uv/normal
		// printf("indind: %d \n", faceInd*9+i*3);
		int vertInd = indices[faceInd*9+i*3];
		// printf("ind: %d \n", vertInd);

		// float4 vertHomo = float4(vertices[vertInd*4],vertices[vertInd*4+1],vertices[vertInd*4+2],vertices[vertInd*4+3]);
		float4 vertHomo = (float4)(vertices[vertInd*3], vertices[vertInd*3+1], vertices[vertInd*3+2], 1.0f);
		// vertHomo = (float4)(vertInd, vertInd+2, 10, 1.0f);
		// vertHomo.y = 4;
		//printVec(vertHomo);
		homoZs[i] = vertHomo.z;
		vertHomo = vec4_mul_mat4(vertHomo, viewport);
		vsVP[i] = vertHomo;
		vs[i] = vertHomo.xyz / vertHomo.w;
	}

	// printf("v1: %f %f %f\n", vs[0].x, vs[0].y, vs[0].z);
	// printf("v2: %f %f %f\n", vs[1].x, vs[1].y, vs[1].z);
	// printf("v3: %f %f %f\n", vs[2].x, vs[2].y, vs[2].z);

	float2 bboxmin = (float2)(INFINITY,INFINITY);
	float2 bboxmax = (float2)(-INFINITY,-INFINITY);

	float2 clampCoords = (float2)(width-1, height-1);

	for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            bboxmin[j] = max(0.f, min(bboxmin[j], vs[i][j]));
            bboxmax[j] = min(clampCoords[j], max(bboxmax[j], vs[i][j]));
        }
    }

    // bboxmin.x = 0;
    // bboxmin.y = 0;
    // bboxmax.x = width;
    // bboxmax.y = height;

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
            //bool discard = shader.fragment(bc_clip, color);
            //if (!discard) {
                zbuffer[pixInd] = frag_depth;
                screen[pixInd*3] = (.5f + .5f * bc_screen.x) * 255;
                screen[pixInd*3+1] = (.5f + .5f * bc_screen.y) * 255;
                screen[pixInd*3+2] = (.5f + .5f * bc_screen.z) * 255;
            //}
        }
    }

    // for (pix.x=0; pix.x<=width; pix.x++) {
    //     for (pix.y=0; pix.y<=height; pix.y++) {
    //     	    int pixInd = pix.x+pix.y*width;
    //             screen[pixInd*3] = 255;
    //             screen[pixInd*3+1] = 100;
    //             screen[pixInd*3+2] = 0;
    //         //}
    //     }
    // }
}

kernel void fragment(
		ulong n,
		int width,
		int height,
		global const float *a,
		global const float *b,
		global char *c
	)
{
	size_t i = get_global_id(0);
	int pix = i / 3;
	int x = pix % width;
	int y = pix / width;

	if (i < n) {
		// c[i] = a[i] + b[i];
		// c[i] = 256 * (((float)i) / n);
		c[i] = 256 * ((float)x + y) / (width + height);
		// c[i] =
	}
}

kernel void vertex(
		ulong n,
		global const float* vertices,
		global float* positions
	)
{
	size_t i = get_global_id(0);
	float x = vertices[i*3];
	float y = vertices[i*3+1];
	float z = vertices[i*3+2];
	float w = 1.0f;

	positions[i*4] = x;
	positions[i*4+1] = y;
	positions[i*4+2] = z;
	positions[i*4+3] = w;
}
